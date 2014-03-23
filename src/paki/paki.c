/***********************************************************************************
 * Copyright (c) 2012, Sepehr Taghdisian
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 ***********************************************************************************/


#include <stdio.h>
#include "dhcore/core.h"
#include "dhcore/zip.h"
#include "dhcore/pak-file.h"

#if defined(_LINUX_) || defined(_OSX_)
#include <dirent.h>
#elif defined(_WIN_)
#include "dhcore/win.h"
#endif

#define VERSION     "1.0"
#define FILE_SIZE_WARNING_THRESHOLD     (64*1024*1024)

/* application input arguments */
enum PAKI_USAGE
{
    PAKI_USAGE_EXTRACT = (1 << 0),
    PAKI_USAGE_COMPRESS = (1 << 1),
    PAKI_USAGE_VERBOSE = (1 << 2),
    PAKI_USAGE_LIST = (1 << 3)
};

struct paki_args
{
    uint usage;  /* PAKI_USAGE combined */
    enum compress_mode compress_mode;
    char path[DH_PATH_MAX];
    char pakfile[DH_PATH_MAX];
    uint err_cnt;
    uint warn_cnt;
    uint file_cnt;
};

/* fwd declarations */
void show_help();
result_t archive_put(struct pak_file* pak, struct paki_args* args,
                     const char* srcfilepath, const char* destfilealias);
result_t compress_directory(struct pak_file* pak, struct paki_args* args, const char* subdir);
void save_pak(struct paki_args* args);
void load_pak(struct paki_args* args);
void list_pak(struct paki_args* args);

//
int main(int argc, char** argv)
{
    struct paki_args args;

    core_init(CORE_INIT_ALL);
    log_outputconsole(TRUE);

    if (argc == 1)  {
        core_release(FALSE);
        return -1;
    }

    // read arguments
    memset(&args, 0x00, sizeof(args));

    args.compress_mode = COMPRESS_NORMAL;
    bool_t invalid_args = FALSE;

    for (int i = 1; i < argc; i++)    {
        if (str_isequal(argv[i], "-x"))      BIT_ADD(args.usage, PAKI_USAGE_EXTRACT);
        else if(str_isequal(argv[i], "-c"))  BIT_ADD(args.usage, PAKI_USAGE_COMPRESS);
        else if(str_isequal(argv[i], "-z0")) args.compress_mode = COMPRESS_NONE;
        else if(str_isequal(argv[i], "-z1")) args.compress_mode = COMPRESS_FAST;
        else if(str_isequal(argv[i], "-z2")) args.compress_mode = COMPRESS_NORMAL;
        else if(str_isequal(argv[i], "-z3")) args.compress_mode = COMPRESS_BEST;
        else if(str_isequal(argv[i], "-v"))  BIT_ADD(args.usage, PAKI_USAGE_VERBOSE);
        else if(str_isequal(argv[i], "-l"))	 BIT_ADD(args.usage, PAKI_USAGE_LIST);
        else if(str_isequal(argv[i], "-h"))  {
        	show_help();
        	core_release(FALSE);
        	return 0;
        }
        else    {
            if (argv[i][0] == '-' || args.path[0] != 0)  {
                /* argument is an unknown parameter */
                invalid_args = TRUE;
                break;
            }

            if (args.pakfile[0] == 0)   strcpy(args.pakfile, argv[i]);
            else                        strcpy(args.path, argv[i]);
        }
    }

    /* check for arguments validity */
    if (invalid_args || args.pakfile[0] == 0 ||
        ((BIT_CHECK(args.usage, PAKI_USAGE_EXTRACT) +
          BIT_CHECK(args.usage, PAKI_USAGE_COMPRESS) +
          BIT_CHECK(args.usage, PAKI_USAGE_LIST)) != 1))
    {
        printf(TERM_BOLDRED "Invalid arguments\n" TERM_RESET);
        core_release(FALSE);
        return -1;
    }

    /* creating archive (-c flag) */
    if (BIT_CHECK(args.usage, PAKI_USAGE_COMPRESS))     {
        save_pak(&args);
    }   else if(BIT_CHECK(args.usage, PAKI_USAGE_EXTRACT))  {
        load_pak(&args);
    } else if(BIT_CHECK(args.usage, PAKI_USAGE_LIST))	{
    	list_pak(&args);
    }

#if defined(_DEBUG_)
    core_release(TRUE);
#else
    core_release(FALSE);
#endif
    return 0;
}

/* show arguments help */
void show_help()
{
    printf("paki - 'pak' file archiver for dark-hammer engine v%s\n", VERSION);
    printf("Usage:\n"
              "  paki [-x] [-c] [-l] [-v] [pakfile] [path]\n"
              "  [-x]: extracts a file defined by [path] from [pak-file]\n"
              "  [-c]: create pak file from [path] including subdirectories.\n"
              "  [-zN]: compression mode. ([-z0]:no-compr [-z1]:fast [-z2]:normal [-z3]:best)\n"
              "  [-l]: list files in the [packfile]\n"
              "  [-v]: verbose mode\n\n");
}

void save_pak(struct paki_args* args)
{
    result_t r;
    struct pak_file pak;

    path_norm(args->pakfile, args->pakfile);
    path_norm(args->path, args->path);

    r = pak_create(&pak, mem_heap(), args->pakfile, args->compress_mode, 0);
    if (IS_FAIL(r))     {
        err_sendtolog(FALSE);
        return;
    }


    r = compress_directory(&pak, args, "");
    if (IS_FAIL(r))     {
        err_sendtolog(FALSE);
        pak_close(&pak);
        return;
    }

    pak_close(&pak);

    // report
    printf(TERM_BOLDWHITE "Saved pak: '%s'\nTotal %d file(s) - %d Error(s), %d Warning(s)\n" TERM_RESET,
               args->pakfile, args->file_cnt, args->err_cnt, args->warn_cnt);
}

#if defined(_LINUX_) || defined(_OSX_)
result_t compress_directory(struct pak_file* pak, struct paki_args* args, const char* subdir)
{
    result_t r;
    char directory[DH_PATH_MAX];
    char filepath[DH_PATH_MAX];
    char fullfilepath[DH_PATH_MAX];

    strcpy(directory, args->path);
    if (subdir[0] != 0)     {
        path_join(directory, path_norm(directory, directory), subdir, NULL);
    }

    DIR* dir = opendir(directory);
    if (dir == NULL)    {
        printf(TERM_BOLDRED "Creating pak failed: directory '%s' does not exist.\n" TERM_RESET,
        		directory);
        return RET_FAIL;
    }

    /* read directory recuresively, and compress files into pak */
    struct dirent* ent = readdir(dir);
    while (ent != NULL)     {
        if (!str_isequal(ent->d_name, ".") && !str_isequal(ent->d_name, ".."))    {
            filepath[0] = 0;
            if (subdir[0] != 0)
               strcpy(filepath, subdir);

            if (ent->d_type != DT_DIR)  {
                // put the file into the archive
                path_join(filepath, filepath, ent->d_name, NULL);
                path_join(fullfilepath, directory, ent->d_name, NULL);
                r = archive_put(pak, args, fullfilepath, filepath);
                if (IS_OK(r) && BIT_CHECK(args->usage, PAKI_USAGE_VERBOSE))     {
                    puts(filepath);
                }   else if (IS_FAIL(r))  {
                    err_sendtolog(FALSE);
                    args->err_cnt ++;
                }
                args->file_cnt ++;
            }   else    {
                // it's a directory, recurse
                path_join(filepath, filepath, ent->d_name, NULL);
                compress_directory(pak, args, filepath);
            }
        }
        ent = readdir(dir);
    }

    closedir(dir);
    return RET_OK;
}
#elif defined(_WIN_)
result_t compress_directory(struct pak_file* pak, struct paki_args* args, const char* subdir)
{
    result_t r;
    char directory[DH_PATH_MAX];
    char filepath[DH_PATH_MAX];
    char fullfilepath[DH_PATH_MAX];

    strcpy(directory, args->path);
    if (subdir[0] != 0)     {
        path_join(directory, directory, subdir, NULL);
    }

    WIN32_FIND_DATA fdata;
    char filter[DH_PATH_MAX];
    path_join(filter, directory, "*", NULL);
    HANDLE find_hdl = FindFirstFile(filter, &fdata);
    if (find_hdl == INVALID_HANDLE_VALUE)    {
        printf(TERM_BOLDRED "Creating pak failed: directory '%s' does not exist.\n" TERM_RESET,
        		directory);
        return RET_FAIL;
    }

    /* read directory recuresively, and compress files into pak */
    BOOL fr = TRUE;
    while (fr)     {
        if (!str_isequal(fdata.cFileName, ".") && !str_isequal(fdata.cFileName, ".."))    {
            filepath[0] = 0;
            if (subdir[0] != 0)     {
                strcpy(filepath, subdir);
            }

            if (!BIT_CHECK(fdata.dwFileAttributes, FILE_ATTRIBUTE_DIRECTORY))  {
                /* put the file into the archive */
                path_join(filepath, filepath, fdata.cFileName, NULL);
                path_join(fullfilepath, directory, fdata.cFileName, NULL);

                r = archive_put(pak, args, fullfilepath, filepath);
                if (IS_OK(r) && BIT_CHECK(args->usage, PAKI_USAGE_VERBOSE))     {
                    puts(filepath);
                }   else if (IS_FAIL(r))  {
                    err_sendtolog(FALSE);
                    args->err_cnt ++;
                }
                args->file_cnt ++;
            }   else    {
                /* it's a directory, recurse */
                path_join(filepath, filepath, fdata.cFileName, NULL);
                compress_directory(pak, args, filepath);
            }
        }
        fr = FindNextFile(find_hdl, &fdata);
    }

    FindClose(find_hdl);
    return RET_OK;
}
#endif

result_t archive_put(struct pak_file* pak, struct paki_args* args,
                     const char* srcfilepath, const char* destfilealias)
{
    result_t r;

    file_t f = fio_opendisk(srcfilepath, TRUE);
    if (f == NULL)     {
        printf(TERM_BOLDRED "Packing file '%s' failed: file may not exist or locked.\n" TERM_RESET,
        		srcfilepath);
        return RET_FILE_ERROR;
    }

    if (fio_getsize(f) > FILE_SIZE_WARNING_THRESHOLD)   {
        printf(TERM_BOLDYELLOW "File '%s' have %dmb of size, which may be too large.\n" TERM_RESET,
        		srcfilepath, (uint)fio_getsize(f)/(1024*1024));
        args->warn_cnt ++;
    }

    char alias[DH_PATH_MAX];
    r = pak_putfile(pak, mem_heap(), f, path_tounix(alias, destfilealias));
    fio_close(f);

    return r;
}

void load_pak(struct paki_args* args)
{
    result_t r;
    struct pak_file pak;
    char filename[DH_PATH_MAX];

    path_norm(args->pakfile, args->pakfile);
    path_tounix(args->path, args->path);

    r = pak_open(&pak, mem_heap(), args->pakfile, 0);
    if (IS_FAIL(r))     {
        err_sendtolog(FALSE);
        return;
    }

    uint file_id = pak_findfile(&pak, args->path);
    if (file_id == INVALID_INDEX)   {
        printf(TERM_BOLDRED "Extract failed: file '%s' not found in pak.\n" TERM_RESET, args->path);
        pak_close(&pak);
        return;
    }

    path_getfullfilename(filename, args->path);
    file_t f = fio_createdisk(filename);
    if (f == NULL)     {
        printf(TERM_BOLDRED "Extract failed: could not create '%s' for writing.\n" TERM_RESET,
        		filename);
        pak_close(&pak);
        err_sendtolog(FALSE);
        return;
    }

    file_t src_file = pak_getfile(&pak, mem_heap(), mem_heap(), file_id, 0);
    if (src_file == NULL)   {
        pak_close(&pak);
        fio_close(f);
        err_sendtolog(FALSE);
        return;
    }

    size_t size;
    struct allocator* alloc;
    void* buffer = fio_detachmem(src_file, &size, &alloc);
    fio_write(f, buffer, size, 1);
    A_FREE(alloc, buffer);
    fio_close(f);

    pak_close(&pak);

    if (BIT_CHECK(args->usage, PAKI_USAGE_VERBOSE)) {
        printf(TERM_WHITE "%s -> %s\n" TERM_RESET, args->path, filename);
    }
    args->file_cnt ++;

    // report
    printf(TERM_BOLDWHITE "Finished: total %d file(s) - %d error(s), %d warning(s)\n" TERM_RESET,
               args->file_cnt, args->err_cnt, args->warn_cnt);
}

void list_pak(struct paki_args* args)
{
	result_t r;
    struct pak_file pak;
    r = pak_open(&pak, mem_heap(), args->pakfile, 0);
    if (IS_FAIL(r))     {
        err_sendtolog(FALSE);
        return;
    }

    uint cnt = INVALID_INDEX;
	char* filelist = pak_createfilelist(&pak, mem_heap(), &cnt);
    pak_close(&pak);
	if (cnt == 0)	{
		printf(TERM_BOLDWHITE "There are no files in the pak '%s'.\n" TERM_RESET, args->pakfile);
		return;
	}

	if (filelist == NULL)	{
		printf(TERM_BOLDRED "Not enough memory.\n" TERM_RESET);
		return;
	}

	for (uint i = 0; i < cnt; i++)	{
		printf(TERM_WHITE "%s\n" TERM_RESET, filelist + i*DH_PATH_MAX);
	}
	printf(TERM_BOLDWHITE "Total %d files in '%s'.\n" TERM_RESET, cnt, args->pakfile);
	FREE(filelist);


}

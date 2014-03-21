import os, sys

VERSION = '0.4.7'

def get_exec_dir(pyfile):
    if sys.executable.lower().find('python') != -1:
        return os.path.dirname(os.path.abspath(pyfile))
    else:
        return os.path.abspath(os.path.dirname(sys.executable))

def make_samefname(out_filepath, in_filepath, file_ext):
    (in_dir, in_nameext) = os.path.split(in_filepath)
    (in_name, in_ext) = os.path.splitext(in_nameext)
    (out_dir, out_nameext) = os.path.split(out_filepath)
    return os.path.join(out_dir, in_name + '.' + file_ext)

def get_rel_path(filepath, rootdir):
    filepath2 = os.path.abspath(filepath).lower()
    rootdir2 = os.path.abspath(rootdir).lower()
    if filepath2.find(rootdir2) != 0:
        return False
    return os.path.normcase(filepath[len(rootdir) + 1:])

def valid_engine_path(filepath):
    filepath = filepath.replace('\\', '/')
    if len(filepath) > 0 and filepath[-1] != '/':
        filepath += '/'
    return filepath
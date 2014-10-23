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
#include "dhcore/array.h"
#include "dhcore/json.h"

#include "assimp/cimport.h"
#include "assimp/postprocess.h"
#include "assimp/scene.h"
#include "assimp/config.h"

#include "dheng/h3d-types.h"

#include "model-import.h"
#include "texture-import.h"
#include "math-conv.h"

/*************************************************************************************************
 * types
 */
struct geo_ext
{
	struct h3d_geo g;

	/* comes after in the h3d file (data) */
	struct h3d_geo_subset* subsets;
	void* indexes;	/* can be uint16* or uint* */
	struct h3d_vertex_base* vbase;
    struct h3d_vertex_skin* vskin;
    struct h3d_vertex_nmap* vnmap;
    struct h3d_vertex_extra* vextra;
	struct h3d_joint* joints;
	struct mat3f* init_pose;
};

struct mtl_ext
{
	struct h3d_mtl m;

	/* comes after in the h3d file (data) */
	struct h3d_texture* textures;
};

struct mesh_ext
{
	struct h3d_mesh m;
	/* comes after in the h3d file (data) */
	struct h3d_submesh* submeshes;
};

struct node_ext
{
	struct h3d_node n;
	/* comes after in the h3d file (data) */
	uint* child_idxs;
};

struct occ_ext
{
    struct h3d_occ o;
    /* data which comes after in the file */
    void* indexes;
    struct vec4f* poss;
};

/*************************************************************************************************
 * forward declarations
 */
struct aiNode* import_find_node(struct aiNode* node, const char* name);
void import_destroy_geo(struct geo_ext* geo);
void import_destroy_mtl(struct mtl_ext* mtl);
void import_destroy_mesh(struct mesh_ext* mesh);
void import_destroy_node(struct node_ext* node);
struct node_ext* import_create_node(const struct aiScene* scene, struct aiNode* node,
		struct array* nodes, struct array* meshes, struct array* geos, struct array* mtls,
		uint parent_id);
struct mesh_ext* import_create_mesh(const struct aiScene* scene, struct array* geos,
		struct array* mtls, const uint* mesh_ids, uint mesh_cnt, int main_node);
struct mtl_ext* import_create_mtl(const struct aiScene* scene, uint mtl_id);
struct geo_ext* import_create_geo(const struct aiScene* scene, const uint* mesh_ids,
    uint mesh_cnt, int main_node);
void import_setup_joints(const struct aiScene* scene, struct h3d_joint* joints,
    struct mat3f* init_pose, struct array* bones,
    struct aiBone** skin_bones, uint skin_bone_cnt, const struct mat3f* root_mat);
uint import_addvertid(uint* ids, uint id_cnt, uint id);
int import_writemodel(const char* filepath, const struct node_ext** nodes, uint node_cnt,
    const struct mesh_ext** meshes, uint mesh_cnt, const struct geo_ext** geos, uint geo_cnt,
    const struct mtl_ext** mtls, uint mtl_cnt, OPTIONAL struct occ_ext* occ);
int import_mtl_texture(const struct aiMaterial* mtl, enum aiTextureType type,
		enum h3d_texture_type mytype, char* filename);
struct occ_ext* import_create_occ(const struct aiScene* scene, struct aiNode* node);
void import_destroy_occ(struct occ_ext* occ);

char* import_get_texture(char* filepath, const struct aiMaterial* mtl, enum aiTextureType type);
void import_listmtls_node(const struct aiScene* scene, const struct aiNode* node);
void import_save_mat(float* r, const struct mat3f* m);

void import_calc_bounds(struct aabb* bb, struct geo_ext* geo);
void import_calc_bounds_skinned(struct aabb* bb, struct geo_ext* geo);
void print_joint(const struct h3d_joint* joints, uint joint_cnt, uint idx, uint level);
void print_joints(const struct h3d_joint* joints, uint joint_cnt);

/*************************************************************************************************
 * inlines
 */
INLINE void import_addbone(struct aiNode* bone, struct array* bones)
{
	for (uint i = 0; i < bones->item_cnt; i++)	{
		struct aiNode* ref_bone = ((struct aiNode**)bones->buffer)[i];
		if (str_isequal(ref_bone->mName.data, bone->mName.data))
			return;
	}
	struct aiNode** pbone = (struct aiNode**)arr_add(bones);
	if (pbone != NULL)
		*pbone = bone;
}

INLINE uint import_findbone(struct array* bones, const char* name)
{
	for (uint i = 0; i < bones->item_cnt; i++)	{
		struct aiNode* ref_bone = ((struct aiNode**)bones->buffer)[i];
		if (str_isequal(ref_bone->mName.data, name))
			return i;
	}
	return INVALID_INDEX;
}

/* if returns NULL, then given bone name doesn't influence any vertex */
INLINE struct aiBone* import_get_skinbone(struct aiBone** skin_bones, uint skin_bone_cnt,
                                          const char* name)
{
    for (uint i = 0; i < skin_bone_cnt; i++)  {
        if (str_isequal(skin_bones[i]->mName.data, name))
            return skin_bones[i];
    }
    return NULL;
}

/*************************************************************************************************
 * globals
 */
enum coord_type g_model_coord;
struct import_params g_import_params;
char g_modeldir[DH_PATH_MAX];
struct aiNode* g_root_node = NULL;
struct mat3f g_model_root;  /* root matrix of the whole model */
struct mat3f g_resize_mat;

/*************************************************************************************************/
int import_list(const struct import_params* params)
{
    memcpy(&g_import_params, params, sizeof(struct import_params));
    path_getdir(g_modeldir, params->in_filepath);

    const struct aiScene* scene = aiImportFile(params->in_filepath, 0);
    if (scene == NULL)	{
        printf(TERM_BOLDRED "Error: (assimp) %s\n" TERM_RESET, aiGetErrorString());
        return FALSE;
    }

    struct aiNode* node = scene->mRootNode;
    if (node == NULL)   {
        aiReleaseImport(scene);
        return FALSE;
    }
    if (node->mNumChildren == 0)    {
        printf("model: %s\n", node->mName.data);
        if (params->list_mtls)
            import_listmtls_node(scene, node);
    }

    int first_one = FALSE;
    for (uint i = 0; i < node->mNumChildren; i++) {
        if (node->mChildren[i]->mNumMeshes > 0) {
            printf("model: %s\n", node->mChildren[i]->mName.data);
            if (!first_one && params->list_mtls) {
                import_listmtls_node(scene, node->mChildren[i]);
                first_one = TRUE;
            }
        }
    }

    aiReleaseImport(scene);
    return TRUE;
}

int import_listmtls(const struct import_params* params)
{
    memcpy(&g_import_params, params, sizeof(struct import_params));
    path_getdir(g_modeldir, params->in_filepath);

    const struct aiScene* scene = aiImportFile(params->in_filepath, 0);
    if (scene == NULL)	{
        printf(TERM_BOLDRED "Error: (assimp) %s\n" TERM_RESET, aiGetErrorString());
        return FALSE;
    }

    struct aiNode* node = import_find_node(scene->mRootNode, params->name);
    if (node == NULL)	{
        printf(TERM_BOLDRED "Error: model name '%s' not found in the file\n" TERM_RESET,
            params->name);
        return FALSE;
    }

    import_listmtls_node(scene, node);

    aiReleaseImport(scene);
    return TRUE;
}

void import_listmtls_node(const struct aiScene* scene, const struct aiNode* node)
{
    for (uint i = 0; i < node->mNumMeshes; i++)   {
        const struct aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        const struct aiMaterial* mtl = scene->mMaterials[mesh->mMaterialIndex];

        char filepath[DH_PATH_MAX];
        json_t jmtl = json_create_obj();

        json_additem_toobj(jmtl, "id", json_create_num((fl64)mesh->mMaterialIndex));
        if (!str_isempty(import_get_texture(filepath, mtl, aiTextureType_DIFFUSE)))
            json_additem_toobj(jmtl, "diffuse", json_create_str(filepath));
        if (!str_isempty(import_get_texture(filepath, mtl, aiTextureType_SHININESS)))
            json_additem_toobj(jmtl, "gloss", json_create_str(filepath));
        if (!str_isempty(import_get_texture(filepath, mtl, aiTextureType_NORMALS)))
            json_additem_toobj(jmtl, "norm", json_create_str(filepath));
        if (!str_isempty(import_get_texture(filepath, mtl, aiTextureType_OPACITY)))
            json_additem_toobj(jmtl, "opacity", json_create_str(filepath));
        if (!str_isempty(import_get_texture(filepath, mtl, aiTextureType_EMISSIVE)))
            json_additem_toobj(jmtl, "emissive", json_create_str(filepath));
        if (!str_isempty(import_get_texture(filepath, mtl, aiTextureType_REFLECTION)))
            json_additem_toobj(jmtl, "reflection", json_create_str(filepath));

        size_t jbuff_sz;
        char* jbuff = json_savetobuffer(jmtl, &jbuff_sz, TRUE);
        printf("material: %s\n", jbuff);
        json_deletebuffer(jbuff);
        json_destroy(jmtl);
    }

    /* recurse for children */
    for (uint i = 0; i < node->mNumChildren; i++)
        import_listmtls_node(scene, node->mChildren[i]);
}

char* import_get_texture(char* filepath, const struct aiMaterial* mtl, enum aiTextureType type)
{
    filepath[0] = 0;
    struct aiString ai_filepath;
    enum aiReturn r = aiGetMaterialString(mtl, AI_MATKEY_TEXTURE(type, 0), &ai_filepath);

    if (r == AI_SUCCESS)	{
        /* if we are using relative paths, append it to model filepath */
        if (ai_filepath.data[0] == '.')
            strcat(strcpy(filepath, g_modeldir), ai_filepath.data + 2);
        else
            strcpy(filepath, ai_filepath.data);
    }

    if (type == aiTextureType_NORMALS && r != AI_SUCCESS)
        return import_get_texture(filepath, mtl, aiTextureType_HEIGHT);

    return filepath;
}


/*************************************************************************************************/
int import_model(const struct import_params* params)
{
	memcpy(&g_import_params, params, sizeof(struct import_params));
    path_getdir(g_modeldir, params->in_filepath);

	/* by default: join identical verts, triangulate, cache optimize, limit bone weights,
	 * and convert to left-handed
	 */
	uint flags =
        aiProcess_JoinIdenticalVertices |
        aiProcess_Triangulate |
		aiProcess_ImproveCacheLocality |
        aiProcess_LimitBoneWeights |
        aiProcess_OptimizeMeshes |
        aiProcess_RemoveRedundantMaterials |
        aiProcess_ValidateDataStructure |
        aiProcess_GenUVCoords |
        aiProcess_TransformUVCoords |
        aiProcess_FlipUVs |
        aiProcess_SortByPType;
    if (params->coord == COORD_NONE)
        flags |= aiProcess_MakeLeftHanded;
    /* for -calctng flag: remove tangent/binormal components before processing */
    if (params->calc_tangents)
        flags |= (aiProcess_CalcTangentSpace | aiProcess_RemoveComponent);

	/* load */
    struct aiPropertyStore* props = aiCreatePropertyStore();
    if (params->calc_tangents)
        aiSetImportPropertyInteger(props, AI_CONFIG_PP_RVC_FLAGS, aiComponent_TANGENTS_AND_BITANGENTS);

	const struct aiScene* scene = aiImportFileExWithProperties(params->in_filepath, flags,
        NULL, props);
    aiReleasePropertyStore(props);

	if (scene == NULL)	{
		printf(TERM_BOLDRED "Error: (assimp) %s\n" TERM_RESET, aiGetErrorString());
		return FALSE;
	}

	struct array nodes;
	struct array meshes;
	struct array geos;
	struct array mtls;
    struct node_ext* mynode;
    struct aiNode* node;
    struct aiNode* occ_node = NULL;
    struct occ_ext* occ = NULL;
    int r;

	if (IS_FAIL(arr_create(mem_heap(), &nodes, sizeof(struct node_ext*), 10, 10, 0)) ||
		IS_FAIL(arr_create(mem_heap(), &meshes, sizeof(struct mesh_ext*), 20, 20, 0)) ||
		IS_FAIL(arr_create(mem_heap(), &geos, sizeof(struct geo_ext*), 20, 20, 0)) ||
		IS_FAIL(arr_create(mem_heap(), &mtls, sizeof(struct mtl_ext*), 30, 30, 0)))
	{
		goto err_cleanup;
	}

	node = import_find_node(scene->mRootNode, params->name);
	if (node == NULL)	{
		printf(TERM_BOLDRED "Error: model name '%s' not found in the file\n" TERM_RESET,
				params->name);
		goto err_cleanup;
	}

    if (!str_isempty(params->occ_name)) {
        occ_node = import_find_node(scene->mRootNode, params->occ_name);
        if (occ_node == NULL)   {
            printf(TERM_BOLDYELLOW "Warning: occluder '%s' not found in the file\n" TERM_RESET,
                params->occ_name);
        }
    }

    /* set globals */
    g_root_node = scene->mRootNode;
    g_model_coord = params->coord;

    /* construct scale matrix */
    mat3_set_ident(&g_resize_mat);
    mat3_set_scalef(&g_resize_mat, params->scale, params->scale, params->scale);

    mynode = import_create_node(scene, node, &nodes, &meshes, &geos, &mtls, INVALID_INDEX);
	if (mynode == NULL)	{
		printf(TERM_BOLDRED "Error: failed to import nodes\n" TERM_RESET);
		goto err_cleanup;
	}

    /* occ */
    if (occ_node != NULL)   {
        occ = import_create_occ(scene, occ_node);
    }

	/* write to file */
	r = import_writemodel(params->out_filepath, (const struct node_ext**)nodes.buffer, nodes.item_cnt,
			(const struct mesh_ext**)meshes.buffer, meshes.item_cnt,
            (const struct geo_ext**)geos.buffer, geos.item_cnt,
            (const struct mtl_ext**)mtls.buffer, mtls.item_cnt,
            occ);
	if (!r)
		goto err_cleanup;

	/* report (verbose mode) */
    if (params->verbose)	{
        printf(TERM_WHITE);
        printf( "Model report:\n"
            "  node count: %d\n"
            "  mesh count: %d\n"
            "  geometry count: %d\n"
            "  material count: %d\n"
            "  occluder: %s\n"
            "  scale: %.2f\n",
            nodes.item_cnt, meshes.item_cnt, geos.item_cnt, mtls.item_cnt,
            (occ != NULL) ? "yes" : "no",
            params->scale);
        uint total_verts = 0;
        uint total_tris = 0;
        uint total_skeleton = 0;
        for (uint i = 0; i < geos.item_cnt; i++)	{
            struct geo_ext* geo = ((struct geo_ext**)geos.buffer)[i];
            total_verts += geo->g.vert_cnt;
            total_tris += geo->g.tri_cnt;
            if (geo->g.joint_cnt > 0)
                total_skeleton ++;
        }
        printf( "  vertex count: %d\n"
            "  triangle count: %d\n"
            "  skeleton mesh count: %d\n", total_verts, total_tris, total_skeleton);
        if (occ != NULL)    {
            printf("  occluder props: %s (vertices=%d, triangles=%d)\n", occ->o.name,
                occ->o.vert_cnt, occ->o.tri_cnt);
        }
        printf("textures:\n");
        for (uint i = 0; i < mtls.item_cnt; i++)	{
            struct mtl_ext* mtl = ((struct mtl_ext**)mtls.buffer)[i];
            for (uint k = 0; k < mtl->m.texture_cnt; k++)
                printf("  (%d) %s: %s\n", (enum h3d_texture_type)i,
                import_get_textureusage((enum h3d_texture_type)mtl->textures[k].type),
                mtl->textures[k].filepath);
        }

        for (uint i = 0; i < geos.item_cnt; i++)	{
            struct geo_ext* geo = ((struct geo_ext**)geos.buffer)[i];
            if (geo->g.joint_cnt > 0)
                print_joints(geo->joints, geo->g.joint_cnt);
        }
        printf(TERM_RESET);
    }

	printf(TERM_BOLDGREEN "ok, saved: \"%s\".\n" TERM_RESET, params->out_filepath);

	/* ok */
	for (uint i = 0; i < nodes.item_cnt; i++)
		import_destroy_node(((struct node_ext**)nodes.buffer)[i]);
	for (uint i = 0; i < meshes.item_cnt; i++)
		import_destroy_mesh(((struct mesh_ext**)meshes.buffer)[i]);
	for (uint i = 0; i < geos.item_cnt; i++)
		import_destroy_geo(((struct geo_ext**)geos.buffer)[i]);
	for (uint i = 0; i < mtls.item_cnt; i++)
		import_destroy_mtl(((struct mtl_ext**)mtls.buffer)[i]);
    if (occ != NULL)
        import_destroy_occ(occ);

	arr_destroy(&nodes);
	arr_destroy(&meshes);
	arr_destroy(&geos);
	arr_destroy(&mtls);
	aiReleaseImport(scene);
	return TRUE;

err_cleanup:
    for (uint i = 0; i < nodes.item_cnt; i++)
        import_destroy_node(((struct node_ext**)nodes.buffer)[i]);
    for (uint i = 0; i < meshes.item_cnt; i++)
        import_destroy_mesh(((struct mesh_ext**)meshes.buffer)[i]);
    for (uint i = 0; i < geos.item_cnt; i++)
        import_destroy_geo(((struct geo_ext**)geos.buffer)[i]);
    for (uint i = 0; i < mtls.item_cnt; i++)
        import_destroy_mtl(((struct mtl_ext**)mtls.buffer)[i]);
    if (occ != NULL)
        import_destroy_occ(occ);
	arr_destroy(&nodes);
	arr_destroy(&meshes);
	arr_destroy(&geos);
	arr_destroy(&mtls);
	if (scene != NULL)
		aiReleaseImport(scene);
	return FALSE;
}


/* recursive ! */
struct aiNode* find_node(struct aiNode* node, const char* name)
{
	if (str_isequal_nocase(node->mName.data, name))
		return node;

	for (uint i = 0; i < node->mNumChildren; i++)	{
		struct aiNode* child = find_node(node->mChildren[i], name);
		if (child != NULL)
			return child;
	}
	return NULL;
}

struct node_ext* import_create_node(const struct aiScene* scene, struct aiNode* node,
		struct array* nodes, struct array* meshes, struct array* geos, struct array* mtls,
		uint parent_id)
{
	struct node_ext* mynode = (struct node_ext*)ALLOC(sizeof(struct node_ext), 0);
	if (mynode == NULL)
		return NULL;
	memset(mynode, 0x00, sizeof(struct node_ext));

	str_safecpy(mynode->n.name, sizeof(mynode->n.name), node->mName.data);
	mynode->n.parent_idx = parent_id;

    struct mat3f local_mat;
    /* if we have parent node, construct model root matrix
     * Model root-matrix multiplies by vertices of the root mesh and also by
     * transform matrices of child nodes (1st child level only)
     */
    if (parent_id == INVALID_INDEX) {
        struct mat3f root_mat;
        import_convert_mat(&root_mat, &g_root_node->mTransformation, g_model_coord);
        import_convert_mat(&g_model_root, &node->mTransformation, g_model_coord);
        mat3_mul(&g_model_root, &g_model_root, &root_mat);
        mat3_mul(&g_model_root, &g_model_root, &g_resize_mat);
        mat3_setm(&local_mat, &g_model_root);
    }   else    {
        import_convert_mat(&local_mat, &node->mTransformation, g_model_coord);

        struct node_ext* parent_node = ((struct node_ext**)nodes->buffer)[parent_id];
        if (parent_node->n.parent_idx == INVALID_INDEX)
            mat3_mul(&local_mat, &local_mat, &g_model_root);
    }

    import_save_mat(mynode->n.local_xform, &local_mat);

	struct aabb bb;
	aabb_setzero(&bb);

	if (node->mNumMeshes > 0)	{
		struct mesh_ext* mesh = import_create_mesh(scene, geos, mtls, node->mMeshes,
            node->mNumMeshes, parent_id == INVALID_INDEX);
		if (mesh == NULL)	{
			FREE(node);
			printf(TERM_BOLDRED "Error: failed to create mesh for '%s'\n" TERM_RESET,
					node->mName.data);
			return NULL;
		}
		struct mesh_ext** pmesh = (struct mesh_ext**)arr_add(meshes);
		if (pmesh == NULL)	{
			FREE(node);
			return NULL;
		}
		*pmesh = mesh;
		mynode->n.mesh_idx = meshes->item_cnt - 1;

		/* make aabb bounding box */
		struct geo_ext* geo = ((struct geo_ext**)geos->buffer)[mesh->m.geo_idx];
        import_calc_bounds(&bb, geo);
	}	else	{
		mynode->n.mesh_idx = INVALID_INDEX;
	}

    import_set3f(mynode->n.bb_min, bb.minpt.f);
    import_set3f(mynode->n.bb_max, bb.maxpt.f);

	/* add node to array */
	struct node_ext** pnode = (struct node_ext**)arr_add(nodes);
	if (pnode == NULL)	{
		FREE(node);
		return NULL;
	}
	*pnode = mynode;
	uint node_idx = nodes->item_cnt - 1;

	/* create child nodes */
	if (node->mNumChildren > 0)	{
		mynode->n.child_cnt = node->mNumChildren;
		mynode->child_idxs = (uint*)ALLOC(sizeof(uint)*node->mNumChildren, 0);
		for (uint i = 0; i < mynode->n.child_cnt; i++)	{
			struct node_ext* child_node = import_create_node(scene, node->mChildren[i],
					nodes, meshes, geos, mtls, node_idx);
			if (child_node == NULL)	{
				printf(TERM_BOLDRED "Error: failed to create node '%s'\n" TERM_RESET,
						node->mChildren[i]->mName.data);
				return NULL;
			}
            mynode->child_idxs[i] = nodes->item_cnt - 1;
		}
	}

	return mynode;
}

struct mesh_ext* import_create_mesh(const struct aiScene* scene, struct array* geos,
		struct array* mtls, const uint* mesh_ids, uint mesh_cnt, int main_node)
{
	struct mesh_ext* mymesh = (struct mesh_ext*)ALLOC(sizeof(struct mesh_ext), 0);
	if (mymesh == NULL)
		return NULL;
	memset(mymesh, 0x00, sizeof(struct mesh_ext));

	/* geometry */
	struct geo_ext* geo = import_create_geo(scene, mesh_ids, mesh_cnt, main_node);
	if (geo == NULL)	{
		printf(TERM_BOLDRED "Error: failed to create geometry\n" TERM_RESET);
		FREE(mymesh);
		return NULL;
	}
	struct geo_ext** pgeo = (struct geo_ext**)arr_add(geos);
	if (pgeo == NULL)	{
		FREE(mymesh);
		return NULL;
	}
	*pgeo = geo;
	mymesh->m.geo_idx = geos->item_cnt - 1;

	/* materials */
	mymesh->m.submesh_cnt = mesh_cnt;
	mymesh->submeshes = (struct h3d_submesh*)ALLOC(sizeof(struct h3d_submesh)*mesh_cnt, 0);
	if (mymesh->submeshes == NULL)	{
		FREE(mymesh);
		return NULL;
	}
	memset(mymesh->submeshes, 0x00, sizeof(struct h3d_submesh));
	for (uint i = 0; i < mesh_cnt; i++)	{
		struct aiMesh* submesh = scene->mMeshes[mesh_ids[i]];
		mymesh->submeshes[i].subset_idx = i;
		struct mtl_ext* mtl = import_create_mtl(scene, submesh->mMaterialIndex);
		if (mtl == NULL)	{
			printf(TERM_BOLDRED "Error: failed to create material\n" TERM_RESET);
			import_destroy_mesh(mymesh);
			return NULL;
		}
		struct mtl_ext** pmtl = (struct mtl_ext**)arr_add(mtls);
		if (pmtl == NULL)	{
			import_destroy_mesh(mymesh);
			return NULL;
		}
		*pmtl = mtl;
		mymesh->submeshes[i].mtl_idx = mtls->item_cnt - 1;
	}

	return mymesh;
}

struct mtl_ext* import_create_mtl(const struct aiScene* scene, uint mtl_id)
{
	struct mtl_ext* mymtl = (struct mtl_ext*)ALLOC(sizeof(struct mtl_ext), 0);
	if (mymtl == NULL)
		return NULL;
	memset(mymtl, 0x00, sizeof(struct mtl_ext));

	struct aiMaterial* mtl = scene->mMaterials[mtl_id];
	ASSERT(mtl != NULL);

    AI_COLOR4D clr;

	/* colors */
    if (aiGetMaterialColor(mtl, AI_MATKEY_COLOR_AMBIENT, &clr) == AI_SUCCESS)
        import_setclr(mymtl->m.ambient, &clr);
    else
        import_set3f1(mymtl->m.ambient, 1.0f);

    if (aiGetMaterialColor(mtl, AI_MATKEY_COLOR_DIFFUSE, &clr) == AI_SUCCESS)
        import_setclr(mymtl->m.diffuse, &clr);
    else
        import_set3f1(mymtl->m.diffuse, 1.0f);

    if (aiGetMaterialColor(mtl, AI_MATKEY_COLOR_SPECULAR, &clr) == AI_SUCCESS)
        import_setclr(mymtl->m.specular, &clr);
    else
        import_set3f1(mymtl->m.specular, 1.0f);

    if (aiGetMaterialColor(mtl, AI_MATKEY_COLOR_EMISSIVE, &clr) == AI_SUCCESS)
        import_setclr(mymtl->m.emissive, &clr);
    else
        import_set3f1(mymtl->m.emissive, 0.0f);

    /* specular params */
    float spec_exp;
    if (aiGetMaterialFloatArray(mtl, AI_MATKEY_SHININESS, &spec_exp, NULL) == AI_SUCCESS)
        mymtl->m.spec_exp = spec_exp/100.0f;
    else
        mymtl->m.spec_exp = 0.5f;

    if (aiGetMaterialFloatArray(mtl, AI_MATKEY_SHININESS_STRENGTH, &mymtl->m.spec_intensity, NULL)
        != AI_SUCCESS)
    {
        mymtl->m.spec_intensity = 1.0f;
    }

    /* transparency */
    if (aiGetMaterialFloatArray(mtl, AI_MATKEY_OPACITY, &mymtl->m.opacity, NULL) != AI_SUCCESS)
        mymtl->m.opacity = 1.0f;
    else
        mymtl->m.opacity = 1.0f - mymtl->m.opacity; /* Note: opacity seems to be misunderstood in assimp */

    /* textures */
    struct h3d_texture textures[6];
    uint tc = 0;
    char filename[DH_PATH_MAX];

    if (import_mtl_texture(mtl, aiTextureType_SHININESS, H3D_TEXTURE_GLOSS, filename))	{
    	textures[tc].type = H3D_TEXTURE_GLOSS;
        path_join(textures[tc].filepath, g_import_params.texture_dir_alias, filename, NULL);
        path_tounix(textures[tc].filepath, textures[tc].filepath);
    	tc ++;
    }

    if (import_mtl_texture(mtl, aiTextureType_DIFFUSE, H3D_TEXTURE_DIFFUSE, filename))	{
    	textures[tc].type = H3D_TEXTURE_DIFFUSE;
        path_join(textures[tc].filepath, g_import_params.texture_dir_alias, filename, NULL);
        path_tounix(textures[tc].filepath, textures[tc].filepath);
    	tc ++;
    }

    if (import_mtl_texture(mtl, aiTextureType_NORMALS, H3D_TEXTURE_NORMAL, filename))	{
    	textures[tc].type = H3D_TEXTURE_NORMAL;
        path_join(textures[tc].filepath, g_import_params.texture_dir_alias, filename, NULL);
        path_tounix(textures[tc].filepath, textures[tc].filepath);
    	tc ++;
    }

    if (import_mtl_texture(mtl, aiTextureType_OPACITY, H3D_TEXTURE_ALPHAMAP, filename))	{
    	textures[tc].type = H3D_TEXTURE_ALPHAMAP;
        path_join(textures[tc].filepath, g_import_params.texture_dir_alias, filename, NULL);
        path_tounix(textures[tc].filepath, textures[tc].filepath);
    	tc ++;
    }

    if (import_mtl_texture(mtl, aiTextureType_EMISSIVE, H3D_TEXTURE_EMISSIVE, filename))	{
    	textures[tc].type = H3D_TEXTURE_EMISSIVE;
        path_join(textures[tc].filepath, g_import_params.texture_dir_alias, filename, NULL);
        path_tounix(textures[tc].filepath, textures[tc].filepath);
    	tc ++;
    }

    if (import_mtl_texture(mtl, aiTextureType_REFLECTION, H3D_TEXTURE_REFLECTION, filename))	{
    	textures[tc].type = H3D_TEXTURE_REFLECTION;
        path_join(textures[tc].filepath, g_import_params.texture_dir_alias, filename, NULL);
        path_tounix(textures[tc].filepath, textures[tc].filepath);
    	tc ++;
    }

    if (tc > 0)	{
    	mymtl->textures = (struct h3d_texture*)ALLOC(sizeof(struct h3d_texture)*tc, 0);
    	if (mymtl->textures == NULL)	{
    		FREE(mymtl);
    		return NULL;
    	}
    	memcpy(mymtl->textures, textures, sizeof(struct h3d_texture)*tc);
    	mymtl->m.texture_cnt = tc;
    }

	return mymtl;
}

int import_mtl_texture(const struct aiMaterial* mtl, enum aiTextureType type,
		enum h3d_texture_type mytype, char* filename)
{
	struct aiString ai_filepath;
	struct import_texture_info info;

	enum aiReturn r = aiGetMaterialString(mtl, AI_MATKEY_TEXTURE(type, 0), &ai_filepath);
	if (r == AI_SUCCESS)	{
        char filepath[DH_PATH_MAX];
        /* if we are using relative paths, append it to model filepath */
        if (ai_filepath.data[0] == '.')
            strcat(strcpy(filepath, g_modeldir), ai_filepath.data + 2);
        else
            strcpy(filepath, ai_filepath.data);

        /* process (compress) textures only if texture-compression is on */
        if (!g_import_params.toff)  {
            path_norm(filepath, filepath);
            if (import_process_texture(filepath, mytype, &g_import_params, filename, &info)) {
                printf(TERM_BOLDWHITE "  ok: %s - %dkb (%dx%d)\n" TERM_RESET, filename,
                    (int)(info.size/1024), info.width, info.height);
                return TRUE;
            }
        }   else    {
            strcat(path_getfilename(filename, filepath), ".dds");
            return TRUE;
        }
	}

	/* for normal maps, try loading from Height */
	if (type == aiTextureType_NORMALS && r != AI_SUCCESS)
		return import_mtl_texture(mtl, aiTextureType_HEIGHT, mytype, filename);

	return FALSE;
}

void import_gather_bones(struct aiNode* root_node, struct array* bones, const char* name)
{
    struct aiNode* node = import_find_node(root_node, name);
    ASSERT(node);
    import_addbone(node, bones);

    /* move up to root_node and add all nodes to bones */
    while (node->mParent != NULL && node->mParent != root_node)   {
        node = node->mParent;
        import_gather_bones(root_node, bones, node->mName.data);
    }
}

void import_gather_bones_childs(struct aiNode* root_node, struct array* bones, const char* name)
{
    struct aiNode* node = import_find_node(root_node, name);
    ASSERT(node);
    import_addbone(node, bones);

    /* add child nodes */
    for (uint i = 0; i < node->mNumChildren; i++) {
        import_gather_bones_childs(root_node, bones, node->mChildren[i]->mName.data);
    }
}

void import_gather_skinbones(struct array* rbones, struct aiBone** bones, uint bone_cnt)
{
    for (uint i = 0; i < bone_cnt; i++)   {
        struct aiBone** p_rbones = (struct aiBone**)rbones->buffer;
        struct aiBone* b = import_get_skinbone(p_rbones, rbones->item_cnt, bones[i]->mName.data);
        if (b == NULL)  {
            p_rbones = (struct aiBone**)arr_add(rbones);
            ASSERT(p_rbones);
            *p_rbones = bones[i];
        }
    }
}

struct geo_ext* import_create_geo(const struct aiScene* scene, const uint* mesh_ids,
    uint mesh_cnt, int main_node)
{
	struct geo_ext* geo = (struct geo_ext*)ALLOC(sizeof(struct geo_ext), 0);
	if (geo == NULL)
		return NULL;
	memset(geo, 0x00, sizeof(struct geo_ext));

	struct array bones;         /* item: aiNode* */
    struct array skin_bones;    /* item: aiBone* */
	uint vert_cnt = 0;
	uint tri_cnt = 0;
	uint idx_offset = 0;
	uint vert_offset = 0;
	uint bones_pervertex_max = 0;
	uint* vert_iw_idxs = NULL;	/* counters for skin indexes of each vertex */
	uint vid_cnt = 0;
    uint indexbuffer_sz;
    int has_skin = FALSE;

	if (IS_FAIL(arr_create(mem_heap(), &bones, sizeof(struct aiNode*), 100, 100, 0)) ||
        IS_FAIL(arr_create(mem_heap(), &skin_bones, sizeof(struct aiBone*), 100, 100, 0)))
    {
		import_destroy_geo(geo);
		return NULL;
	}

	for (uint i = 0; i < mesh_cnt; i++)	{
		struct aiMesh* submesh = scene->mMeshes[mesh_ids[i]];
		vert_cnt += submesh->mNumVertices;
		tri_cnt += submesh->mNumFaces;

        import_gather_skinbones(&skin_bones, submesh->mBones, submesh->mNumBones);

        for (uint k = 0; k < submesh->mNumBones; k++) {
            import_gather_bones(scene->mRootNode, &bones, submesh->mBones[k]->mName.data);
		}
        for (uint k = 0; k < submesh->mNumBones; k++) {
            import_gather_bones_childs(scene->mRootNode, &bones, submesh->mBones[k]->mName.data);
        }

		ASSERT(submesh->mNumVertices > 0);
        has_skin |= (submesh->mNumBones > 0);
	}

	if (vert_cnt == 0 || tri_cnt == 0)	{
		printf(TERM_BOLDRED "Error: geometry has no face or vertex!\n" TERM_RESET);
		goto err_cleanup;
	}

	geo->g.tri_cnt = tri_cnt;
	geo->g.vert_cnt = vert_cnt;
	geo->g.subset_cnt = mesh_cnt;
	geo->subsets = (struct h3d_geo_subset*)ALLOC(sizeof(struct h3d_geo_subset)*mesh_cnt, 0);
	geo->g.ib_isui32 = (vert_cnt > UINT16_MAX);
	indexbuffer_sz = geo->g.ib_isui32 ? sizeof(uint)*tri_cnt*3 : sizeof(uint16)*tri_cnt*3;
	geo->indexes = ALLOC(indexbuffer_sz, 0);
	if (geo->subsets == NULL || geo->indexes == NULL)
		goto err_cleanup;

	memset(geo->subsets, 0x00, sizeof(struct h3d_geo_subset)*mesh_cnt);
	memset(geo->indexes, 0x00, indexbuffer_sz);

	/* vertex IDs */
	for (uint i = 0; i < mesh_cnt; i++)	{
		struct aiMesh* submesh = scene->mMeshes[mesh_ids[i]];

        /* base group */
        ASSERT(submesh->mVertices != NULL);
        vid_cnt = import_addvertid(geo->g.vert_ids, vid_cnt, GFX_INPUTELEMENT_ID_POSITION);
        if (geo->vbase == NULL) {
            geo->vbase =
                (struct h3d_vertex_base*)ALIGNED_ALLOC(sizeof(struct h3d_vertex_base)*vert_cnt, 0);
        }

        if (submesh->mNormals != NULL)
	        vid_cnt = import_addvertid(geo->g.vert_ids, vid_cnt, GFX_INPUTELEMENT_ID_NORMAL);

        if (submesh->mTextureCoords[0] != NULL)
            vid_cnt = import_addvertid(geo->g.vert_ids, vid_cnt, GFX_INPUTELEMENT_ID_TEXCOORD0);

        /* skinning group */
        if (submesh->mBones != NULL && submesh->mNumBones > 0)	{
            vid_cnt = import_addvertid(geo->g.vert_ids, vid_cnt, GFX_INPUTELEMENT_ID_BLENDWEIGHT);
            vid_cnt = import_addvertid(geo->g.vert_ids, vid_cnt, GFX_INPUTELEMENT_ID_BLENDINDEX);

            if (geo->vskin == NULL) {
                geo->vskin = (struct h3d_vertex_skin*)
                    ALIGNED_ALLOC(sizeof(struct h3d_vertex_skin)*vert_cnt, 0);
                memset(geo->vskin, 0x00, sizeof(struct h3d_vertex_skin)*vert_cnt);
            }
        }

        /* normal-map group */
		if (submesh->mTangents != NULL && submesh->mBitangents != NULL) {
			vid_cnt = import_addvertid(geo->g.vert_ids, vid_cnt, GFX_INPUTELEMENT_ID_TANGENT);
			vid_cnt = import_addvertid(geo->g.vert_ids, vid_cnt, GFX_INPUTELEMENT_ID_BINORMAL);
			if (geo->vnmap == NULL) {
				geo->vnmap = (struct h3d_vertex_nmap*)
                    ALIGNED_ALLOC(sizeof(struct h3d_vertex_nmap)*vert_cnt, 0);
            }
		}

        /* extra group */
        if ((submesh->mTextureCoords[1] != NULL || submesh->mColors[0] != NULL) &&
            geo->vextra == NULL)
        {
            geo->vextra = (struct h3d_vertex_extra*)
                ALIGNED_ALLOC(sizeof(struct h3d_vertex_extra)*vert_cnt, 0);
        }

		if (submesh->mTextureCoords[1] != NULL)
			vid_cnt = import_addvertid(geo->g.vert_ids, vid_cnt, GFX_INPUTELEMENT_ID_TEXCOORD1);

		if (submesh->mColors[0] != NULL)
			vid_cnt = import_addvertid(geo->g.vert_ids, vid_cnt, GFX_INPUTELEMENT_ID_COLOR);
	}

	/* joint data and temp memory for bone referencing */
	if (geo->vskin != NULL)	{
		vert_iw_idxs = (uint*)ALLOC(sizeof(uint)*vert_cnt, 0);
		if (vert_iw_idxs == NULL)
			goto err_cleanup;

		memset(vert_iw_idxs, 0x00, sizeof(uint)*vert_cnt);

		/* setup joints */
		geo->g.joint_cnt = bones.item_cnt;
		geo->joints = (struct h3d_joint*)ALIGNED_ALLOC(sizeof(struct h3d_joint)*bones.item_cnt, 0);
		geo->init_pose = (struct mat3f*)ALIGNED_ALLOC(sizeof(struct mat3f)*bones.item_cnt, 0);
		if (geo->joints == NULL || geo->joints == NULL)
			goto err_cleanup;

        struct mat3f root_mat;
        import_convert_mat(&root_mat, &g_root_node->mTransformation, g_model_coord);

		import_setup_joints(scene, geo->joints, geo->init_pose, &bones,
            (struct aiBone**)skin_bones.buffer, skin_bones.item_cnt, &root_mat);

        import_save_mat(geo->g.joints_rootmat, &root_mat);
	}

	for (uint i = 0; i < mesh_cnt; i++)	{
		struct aiMesh* submesh = scene->mMeshes[mesh_ids[i]];

		/* indexes */
		if (!geo->g.ib_isui32)	{
			uint16* indexes = (uint16*)geo->indexes;
			for (uint k = 0; k < submesh->mNumFaces; k++)	{
				ASSERT(submesh->mFaces[k].mNumIndices == 3);
				uint idx = k*3 + idx_offset;

				indexes[idx] = (uint16)(submesh->mFaces[k].mIndices[0] + vert_offset);
				indexes[idx + 1] = (uint16)(submesh->mFaces[k].mIndices[1] + vert_offset);
				indexes[idx + 2] = (uint16)(submesh->mFaces[k].mIndices[2] + vert_offset);
			}
		}	else	{
			uint* indexes = (uint*)geo->indexes;
			for (uint k = 0; k < submesh->mNumFaces; k++)	{
				ASSERT(submesh->mFaces[k].mNumIndices == 3);
				uint idx = k*3 + idx_offset;

				indexes[idx] = submesh->mFaces[k].mIndices[0] + vert_offset;
				indexes[idx + 1] = submesh->mFaces[k].mIndices[1] + vert_offset;
				indexes[idx + 2] = submesh->mFaces[k].mIndices[2] + vert_offset;
			}
		}

		/* vertices
		 * Transform them only for main(root) node, skinned meshes are automatically transfomed by
		 * their bones */
        struct mat3f root_mat;
        if (!has_skin && main_node)  {
            mat3_setm(&root_mat, &g_model_root);
        }   else    {
            mat3_set_ident(&root_mat);
        }

		for (uint k = 0; k < submesh->mNumVertices; k++)	{
            uint idx = k + vert_offset;
			if (submesh->mVertices != NULL)	{
				const AI_VECTOR3D* p = &submesh->mVertices[k];
                struct vec3f* vp = &geo->vbase[idx].pos;
                vec3_setf(vp, p->x, p->y, p->z);
                import_convert_vec3(vp, vp, g_model_coord);
                vec3_transformsrt(vp, vp, &root_mat);
			}

			if (submesh->mNormals != NULL)	{
				const AI_VECTOR3D* p = &submesh->mNormals[k];
                struct vec3f* vn = &geo->vbase[idx].norm;
				vec3_setf(vn, p->x, p->y, p->z);
                import_convert_vec3(vn, vn, g_model_coord);
                vec3_transformsr(vn, vn, &root_mat);
			}


            if (submesh->mTextureCoords[0] != NULL)	{
                const AI_VECTOR3D* t = &submesh->mTextureCoords[0][k];
                struct vec2f* vt = &geo->vbase[idx].coord;
                vec2f_setf(vt, t->x, t->y);
            }

			if (submesh->mTangents != NULL && submesh->mBitangents != NULL)	{
				const AI_VECTOR3D* pt = &submesh->mTangents[k];
				const AI_VECTOR3D* pb = &submesh->mBitangents[k];
                struct vec3f* vpt = &geo->vnmap[idx].tangent;
                struct vec3f* vpb = &geo->vnmap[idx].binorm;

                vec3_setf(vpt, pt->x, pt->y, pt->z);
                import_convert_vec3(vpt, vpt, g_model_coord);
                vec3_transformsr(vpt, vpt,  &root_mat);

                vec3_setf(vpb, pb->x, pb->y, pb->z);
                import_convert_vec3(vpb, vpb, g_model_coord);
                vec3_transformsr(vpb, vpb,  &root_mat);
			}


            if (submesh->mTextureCoords[1] != NULL)	{
                const AI_VECTOR3D* t = &submesh->mTextureCoords[1][k];
                struct vec2f* vt = &geo->vextra[idx].coord2;
                vec2f_setf(vt, t->x, t->y);
            }

			if (submesh->mColors[0] != NULL)	{
				const AI_COLOR4D* c = &submesh->mColors[0][k];
                struct color* vc = &geo->vextra[idx].color;
				color_setf(vc, c->r, c->g, c->b, c->a);
			}
		} /* for each vertex */

		/* skin data - for each bone ... */
		if (submesh->mBones != NULL && submesh->mNumBones > 0)	{
			for (uint k = 0; k < submesh->mNumBones; k++)	{
				struct aiBone* bone = submesh->mBones[k];
				uint bone_idx = import_findbone(&bones, bone->mName.data);
				ASSERT(bone_idx != INVALID_INDEX);
				for (uint c = 0; c < bone->mNumWeights; c++)	{
					uint vert_idx = bone->mWeights[c].mVertexId + vert_offset;
					geo->vskin[vert_idx].indices.n[vert_iw_idxs[vert_idx]] = bone_idx;
					geo->vskin[vert_idx].weights.f[vert_iw_idxs[vert_idx]] =
							bone->mWeights[c].mWeight;
					vert_iw_idxs[vert_idx] ++;

					if (vert_iw_idxs[vert_idx] > bones_pervertex_max)
						bones_pervertex_max = vert_iw_idxs[vert_idx];
				}
			}
		}

		geo->g.vert_id_cnt = vid_cnt;
        geo->g.bones_pervertex_max = bones_pervertex_max;

		/* subsets */
		geo->subsets[i].ib_idx = idx_offset;
		geo->subsets[i].idx_cnt = submesh->mNumFaces * 3;

		/* progress to next mesh */
		idx_offset += submesh->mNumFaces*3;
		vert_offset += submesh->mNumVertices;
	}

    arr_destroy(&skin_bones);
	arr_destroy(&bones);
	if (vert_iw_idxs != NULL)
		FREE(vert_iw_idxs);
	return geo;

err_cleanup:
    arr_destroy(&skin_bones);
	arr_destroy(&bones);
	if (vert_iw_idxs != NULL)
		FREE(vert_iw_idxs);
	if (geo != NULL)
		import_destroy_geo(geo);
	return NULL;
}


struct occ_ext* import_create_occ(const struct aiScene* scene, struct aiNode* node)
{
    uint vert_cnt = 0;
    uint tri_cnt = 0;

    struct occ_ext* myocc = (struct occ_ext*)ALLOC(sizeof(struct occ_ext), 0);
    if (myocc == NULL)
        return NULL;
    memset(myocc, 0x00, sizeof(struct occ_ext));

    /* calculate final transform matrice that we have to transform our vertices to */
    struct mat3f xform_mat;
    struct mat3f tmp_mat;
    import_convert_mat(&xform_mat, &node->mTransformation, g_model_coord);
    struct aiNode* pnode = node->mParent;
    while (pnode != NULL)    {
        mat3_mul(&xform_mat, &xform_mat,
            import_convert_mat(&tmp_mat, &pnode->mTransformation, g_model_coord));
        pnode = pnode->mParent;
    }
    mat3_mul(&xform_mat, &xform_mat, &g_resize_mat);

    for (uint i = 0; i < node->mNumMeshes; i++)	{
        struct aiMesh* submesh = scene->mMeshes[node->mMeshes[i]];
        vert_cnt += submesh->mNumVertices;
        tri_cnt += submesh->mNumFaces;
        ASSERT(submesh->mNumVertices > 0);
    }

    str_safecpy(myocc->o.name, sizeof(myocc->o.name), node->mName.data);
    myocc->o.tri_cnt = tri_cnt;
    myocc->o.vert_cnt = vert_cnt;
    if (tri_cnt*3 > UINT16_MAX) {
        FREE(myocc);
        printf(TERM_BOLDYELLOW "Warning: ignoring occluder-mesh '%s', too many triangles\n" TERM_RESET,
            node->mName.data);
        return NULL;
    }

    myocc->indexes = ALLOC(sizeof(uint16)*tri_cnt*3, 0);
    if (myocc->indexes == NULL) {
        FREE(myocc);
        return NULL;
    }
    memset(myocc->indexes, 0x00, sizeof(uint16)*tri_cnt*3);

    myocc->poss = (struct vec3f*)ALIGNED_ALLOC(sizeof(struct vec3f)*vert_cnt, 0);
    if (myocc->poss == NULL)    {
        FREE(myocc->indexes);
        FREE(myocc);
        return NULL;
    }
    memset(myocc->poss, 0x00, sizeof(struct vec3f)*vert_cnt);

    /* fill the data */
    uint vert_offset = 0;
    uint idx_offset = 0;

    for (uint i = 0; i < node->mNumMeshes; i++)   {
        struct aiMesh* submesh = scene->mMeshes[node->mMeshes[i]];
        /* indexes */
        uint16* indexes = (uint16*)myocc->indexes;
        for (uint k = 0; k < submesh->mNumFaces; k++)	{
            ASSERT(submesh->mFaces[k].mNumIndices == 3);
            uint idx = k*3 + idx_offset;

            indexes[idx] = (uint16)(submesh->mFaces[k].mIndices[0] + vert_offset);
            indexes[idx + 1] = (uint16)(submesh->mFaces[k].mIndices[1] + vert_offset);
            indexes[idx + 2] = (uint16)(submesh->mFaces[k].mIndices[2] + vert_offset);
        }

        /* vertices */
        for (uint k = 0; k < submesh->mNumVertices; k++)	{
            uint idx = k + vert_offset;
            const AI_VECTOR3D* p = &submesh->mVertices[k];
            vec3_setf(&myocc->poss[idx], p->x, p->y, p->z);
            import_convert_vec3(&myocc->poss[idx], &myocc->poss[idx], g_model_coord);
            vec3_transformsrt(&myocc->poss[idx], &myocc->poss[idx], &xform_mat);
        }

        /* progress */
        idx_offset += submesh->mNumFaces*3;
        vert_offset += submesh->mNumVertices;
    }

    return myocc;
}

void import_setup_joints(const struct aiScene* scene, struct h3d_joint* joints,
                         struct mat3f* init_pose, struct array* bones,
                         struct aiBone** skin_bones, uint skin_bone_cnt,
                         const struct mat3f* root_mat)
{
    struct mat3f offset_mat;

	for (uint i = 0; i < bones->item_cnt; i++)	{
		struct aiNode* bone = ((struct aiNode**)bones->buffer)[i];
		str_safecpy(joints[i].name, sizeof(joints[i].name), bone->mName.data);

        /* try to find it in the skin bones, if doesn't exist, we set offsetmat to identity
         * because it won't influence any vertices */
        struct aiBone* skin_bone = import_get_skinbone(skin_bones, skin_bone_cnt, bone->mName.data);
        if (skin_bone != NULL)   {
            import_convert_mat(&offset_mat, &skin_bone->mOffsetMatrix, g_model_coord);
        }   else    {
            mat3_set_ident(&offset_mat);
        }

        import_save_mat(joints[i].offset_mat, &offset_mat);
		joints[i].parent_idx = INVALID_INDEX;

        /* resolve parent index and transformation of the joint
         * We multiply the root bones by root_mat to properly transform skeleton into model-space */
        struct aiNode* joint_node = import_find_node(scene->mRootNode, bone->mName.data);
		if (joint_node != NULL)	{
			joints[i].parent_idx = (joint_node->mParent != NULL) ?
					import_findbone(bones, joint_node->mParent->mName.data) : INVALID_INDEX;

            import_convert_mat(&init_pose[i], &joint_node->mTransformation, g_model_coord);
            if (joint_node->mParent == g_root_node) {
                mat3_mul(&init_pose[i], &init_pose[i], root_mat);
                mat3_mul(&init_pose[i], &init_pose[i], &g_resize_mat);
            }
		}
	}
}

uint import_addvertid(uint* ids, uint id_cnt, uint id)
{
	for (uint i = 0; i < id_cnt; i++)	{
		if (ids[i] == id)
			return id_cnt;
	}
	ids[id_cnt++] = id;
	return id_cnt;
}

/* recursive */
struct aiNode* import_find_node(struct aiNode* node, const char* name)
{
	if (str_isequal_nocase(node->mName.data, name))
		return node;

	for (uint i = 0; i < node->mNumChildren; i++)	{
		struct aiNode* child = import_find_node(node->mChildren[i], name);
		if (child != NULL)
			return child;
	}
	return NULL;
}

void import_destroy_geo(struct geo_ext* geo)
{
	if (geo->vbase != NULL)
		ALIGNED_FREE(geo->vbase);
	if (geo->vskin != NULL)
		ALIGNED_FREE(geo->vskin);
	if (geo->vnmap != NULL)
		ALIGNED_FREE(geo->vnmap);
	if (geo->vextra != NULL)
		ALIGNED_FREE(geo->vextra);

	if (geo->indexes != NULL)
		FREE(geo->indexes);
	if (geo->subsets != NULL)
		FREE(geo->subsets);
	if (geo->init_pose != NULL)
		ALIGNED_FREE(geo->init_pose);
	if (geo->joints != NULL)
		ALIGNED_FREE(geo->joints);

	FREE(geo);
}

void import_destroy_mtl(struct mtl_ext* mtl)
{
	if (mtl->textures != NULL)
		FREE(mtl->textures);
	FREE(mtl);
}

void import_destroy_mesh(struct mesh_ext* mesh)
{
	if (mesh->submeshes != NULL)
		FREE(mesh->submeshes);
	FREE(mesh);
}

void import_destroy_node(struct node_ext* node)
{
	if (node->child_idxs != NULL)
		FREE(node->child_idxs);
	FREE(node);
}

void import_destroy_occ(struct occ_ext* occ)
{
    if (occ->indexes != NULL)
        FREE(occ->indexes);
    if (occ->poss != NULL)
        ALIGNED_FREE(occ->poss);
    FREE(occ);
}

int import_writemodel(const char* filepath, const struct node_ext** nodes, uint node_cnt,
		const struct mesh_ext** meshes, uint mesh_cnt, const struct geo_ext** geos, uint geo_cnt,
		const struct mtl_ext** mtls, uint mtl_cnt, OPTIONAL struct occ_ext* occ)
{
    /* write to temp file */
    char filepath_tmp[DH_PATH_MAX];
    strcat(strcpy(filepath_tmp, filepath), ".tmp");
	FILE* f = fopen(filepath_tmp, "wb");
	if (f == NULL)	{
		printf(TERM_BOLDRED "Error: failed to open file '%s' for writing\n" TERM_RESET, filepath);
		return FALSE;
	}

	struct h3d_header header;
	header.sign = H3D_SIGN;
	header.type = H3D_MESH;
	header.version = H3D_VERSION_13;
	header.data_offset = sizeof(struct h3d_header);
	fwrite(&header, sizeof(header), 1, f);

    /* model info */
    struct h3d_model model;
    memset(&model, 0x00, sizeof(model));
    model.node_cnt = node_cnt;
    model.mesh_cnt = mesh_cnt;
    model.geo_cnt = geo_cnt;
    model.mtl_cnt = mtl_cnt;

    for (uint i = 0; i < node_cnt; i++)
        model.total_childidxs += nodes[i]->n.child_cnt;
    for (uint i = 0; i < mesh_cnt; i++)
        model.total_submeshes += meshes[i]->m.submesh_cnt;
    for (uint i = 0; i < mtl_cnt; i++)
        model.total_maps += mtls[i]->m.texture_cnt;
    for (uint i = 0; i < geo_cnt; i++)    {
        model.total_joints += geos[i]->g.joint_cnt;
        model.total_geo_subsets += geos[i]->g.subset_cnt;
        if (geos[i]->g.joint_cnt > 0)
            model.total_skeletons ++;
    }

    if (occ != NULL)    {
        model.has_occ = TRUE;
        model.occ_idx_cnt = occ->o.tri_cnt*3;
        model.occ_vert_cnt = occ->o.vert_cnt;
    }

    fwrite(&model, sizeof(model), 1, f);

	/* nodes */
	if (nodes != NULL)	{
		for (uint i = 0; i < node_cnt; i++)	{
			const struct node_ext* node = nodes[i];
			fwrite(&node->n, sizeof(struct h3d_node), 1, f);
			if (node->n.child_cnt > 0 && node->child_idxs != NULL)
				fwrite(node->child_idxs, sizeof(uint), node->n.child_cnt, f);
		}
	}

	/* meshes */
	if (meshes != NULL)	{
		for (uint i = 0; i < mesh_cnt; i++)	{
			const struct mesh_ext* mesh = meshes[i];
			fwrite(&mesh->m, sizeof(struct h3d_mesh), 1, f);
			if (mesh->m.submesh_cnt > 0 && mesh->submeshes != NULL)
				fwrite(mesh->submeshes, sizeof(struct h3d_submesh), mesh->m.submesh_cnt, f);
		}
	}

	/* geos */
	if (geos != NULL)	{
		for (uint i = 0; i < geo_cnt; i++)	{
			const struct geo_ext* geo = geos[i];
			fwrite(&geo->g, sizeof(struct h3d_geo), 1, f);
			if (geo->subsets != NULL && geo->g.subset_cnt > 0)
				fwrite(geo->subsets, sizeof(struct h3d_geo_subset), geo->g.subset_cnt, f);
			if (geo->indexes != NULL && geo->g.tri_cnt > 0)	{
				fwrite(geo->indexes, geo->g.ib_isui32 ? sizeof(uint) : sizeof(uint16),
						geo->g.tri_cnt*3, f);
			}

			if (geo->vbase != NULL)
				fwrite(geo->vbase, sizeof(struct h3d_vertex_base), geo->g.vert_cnt, f);

            if (geo->vskin != NULL)
                fwrite(geo->vskin, sizeof(struct h3d_vertex_skin), geo->g.vert_cnt, f);

            if (geo->vnmap != NULL)
                fwrite(geo->vnmap, sizeof(struct h3d_vertex_nmap), geo->g.vert_cnt, f);

			if (geo->vextra != NULL)
				fwrite(geo->vextra, sizeof(struct h3d_vertex_extra), geo->g.vert_cnt, f);

			if (geo->joints != NULL && geo->g.joint_cnt > 0)	{
				fwrite(geo->joints, sizeof(struct h3d_joint), geo->g.joint_cnt, f);
				fwrite(geo->init_pose, sizeof(struct mat3f), geo->g.joint_cnt, f);
			}
		}
	}

	/* materials */
	if (mtls != NULL)	{
		for (uint i = 0; i < mtl_cnt; i++)	{
			const struct mtl_ext* mtl = mtls[i];
			fwrite(&mtl->m, sizeof(struct h3d_mtl), 1, f);
			if (mtl->textures != NULL && mtl->m.texture_cnt > 0)	{
				fwrite(mtl->textures, sizeof(struct h3d_texture), mtl->m.texture_cnt, f);
			}
		}
	}

    /* occ */
    if (occ != NULL)    {
        fwrite(&occ->o, sizeof(struct h3d_occ), 1, f);
        fwrite(occ->indexes, sizeof(uint16), occ->o.tri_cnt*3, f);
        fwrite(occ->poss, sizeof(struct vec3f), occ->o.vert_cnt, f);
    }

	fclose(f);

    /* move temp to main file */
    return util_movefile(filepath, filepath_tmp);
}

void print_joints(const struct h3d_joint* joints, uint joint_cnt)
{
    puts("Joints:\n");
    for (uint i = 0; i < joint_cnt; i++)  {
        const struct h3d_joint* j = &joints[i];
        if (j->parent_idx == INVALID_INDEX)
            print_joint(joints, joint_cnt, i, 0);
    }
}

void print_joint(const struct h3d_joint* joints, uint joint_cnt, uint idx, uint level)
{
    const struct h3d_joint* j = &joints[idx];

    char level_str[64];
    level_str[0] = 0;
    for (uint i = 0; i < level; i++)
        strcat(level_str, "  ");
    printf("%s- %s (idx=%d)\n", level_str, j->name, idx);

    for (uint i = 0; i < joint_cnt; i++)  {
        if (joints[i].parent_idx == idx)    {
            print_joint(joints, joint_cnt, i, level + 1);
        }
    }
}

void import_calc_bounds(struct aabb* bb, struct geo_ext* geo)
{
    if (geo->g.joint_cnt == 0)  {
        for (uint i = 0; i < geo->g.vert_cnt; i++)
            aabb_pushptv(bb, &geo->vbase[i].pos);
    }   else    {
        import_calc_bounds_skinned(bb, geo);
    }
}

/* apply skinning and then push the vertices into bounding box */
void import_calc_bounds_skinned(struct aabb* bb, struct geo_ext* geo)
{
    uint joint_cnt = geo->g.joint_cnt;
    ASSERT(joint_cnt > 0);

    struct mat3f* skin_mats = (struct mat3f*)ALIGNED_ALLOC(sizeof(struct mat3f)*joint_cnt, 0);
    ASSERT(skin_mats);

    const struct mat3f* init_pose = geo->init_pose;
    struct mat3f offset_mat;

    for (uint i = 0; i < geo->g.joint_cnt; i++)   {
        /* calculate model-space joint mat */
        struct h3d_joint* joint = &geo->joints[i];
        mat3_setm(&skin_mats[i], &init_pose[i]);

        while (joint->parent_idx != INVALID_INDEX)  {
            mat3_mul(&skin_mats[i], &skin_mats[i], &init_pose[joint->parent_idx]);
            joint = &geo->joints[joint->parent_idx];
        }

        /* construct offset matrix for the joint and we transform it by joint mat */
        const float* off = geo->joints[i].offset_mat;
        mat3_setf(&offset_mat,
            off[0], off[1], off[2],
            off[3], off[4], off[5],
            off[6], off[7], off[8],
            off[9], off[10], off[11]);
        mat3_mul(&skin_mats[i], &offset_mat, &skin_mats[i]);
    }

    /* apply skinning mats to vertices */
    struct vec3f skinned_pos;
    struct vec3f tmpv;
    for (uint i = 0; i < geo->g.vert_cnt; i++)    {
        const struct vec3f* pos = &geo->vbase[i].pos;
        const struct vec4i* b_indexes = &geo->vskin[i].indices;
        const struct vec4f* b_weights = &geo->vskin[i].weights;

        vec3_setzero(&skinned_pos);
        for (uint c = 0; c < 4; c++)  {
            struct mat3f* mat = &skin_mats[b_indexes->n[c]];
            const float w = b_weights->f[c];

            vec3_muls(&tmpv, vec3_transformsrt(&tmpv, pos, mat), w);
            vec3_add(&skinned_pos, &skinned_pos, &tmpv);

            aabb_pushptv(bb, &skinned_pos);
        }
    }

    ALIGNED_FREE(skin_mats);
}


#ifndef C_MODEL_LOADER
#define C_MODEL_LOADER

#include "engine.h"
#include "platform.h"

#include <glad/glad.h>
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

struct VertexBufferAttribute
{
	u8 location;
	u8 componentCount;
	u8 offset;
};

struct VertexBufferLayout
{
	std::vector< VertexBufferAttribute> attributes;
	u8 stride;
};

struct Model
{
	u32 meshIdx;
	std::vector<u32> materialIdx;
};
struct SubMesh
{
	VertexBufferLayout vertexBufferLayout;
	std::vector<float> vertices;
	std::vector<u32> indices;
	u32 vertexOffset;
	u32 indexOffset;
	std::vector<Vao> vaos;

};
struct Mesh
{
	std::vector<SubMesh> submeshes;
	GLuint vertexBufferHandle;
	GLuint indexBufferHandle;
};
struct Material
{
	std::string name;
	vec3 albedo;
	vec3 emissive;
	f32 smothness;
	u32 albedoTextureIdx;
	u32 emissiveTextureIdx;
	u32 specularTextureIdx;
	u32 normalTextureIdx;
	u32 bumpTextureIdx;

};


void ProcessAssimpMesh(const aiScene* scene, aiMesh* mesh, Mesh* myMesh, u32 baseMeshMaterialIndex, std::vector<u32>& submeshMaterialIndices);

void ProcessAssimpMaterial(App* app, aiMaterial* material, Material& myMaterial, String directory);

void ProcessAssimpNode(const aiScene* scene, aiNode* node, Mesh* myMesh, u32 baseMeshMaterialIndex, std::vector<u32>& submeshMaterialIndices);

u32 LoadModel(App* app, const char* filename);
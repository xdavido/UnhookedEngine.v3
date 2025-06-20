#pragma once

#include "platform.h"
#include "ModelLoader.h"
#include "BufferManagement.h"
#include "Structs.h"

#include <glad/glad.h>



void Init(App* app);

void Gui(App* app);

void UpdateEntities(App* app);

void Update(App* app);

void UpdateLights(App* app);

void Render(App* app);

void CleanUp(App* app);

GLuint FindVAO(Mesh& mesh, u32 submeshIndex, const Program& program);

u32 LoadTexture2D(App* app, const char* filepath);

glm::mat4 TransformScale(const vec3& scaleFactors);

glm::mat4 TransformPositionScale(const vec3& pos, const vec3& scaleFactor);

void CreateEntity(App* app, const u32 aModelIdx, const glm::mat4& aVP, const glm::vec3& aPos);

void RenderScreenFillQuad(App* app, const FrameBuffer& aFBO);

void RenderWater(App* app);

void RenderSkybox(App* app, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);

void RenderCube(App* app);

void RenderSceneWithClipPlane(App* app, const glm::vec4& clipPlane);

u32 ConvertHDRIToCubemap(App* app, const char* filepath);

void CreateIrradianceMap(App* app);

void CreateIrradianceMap(App* app);

GLuint CreateCubeVAO(App* app);

std::vector<std::string> GetHDRIFiles(const std::string& folderPath);

static std::vector<std::string> hdriFiles;
static int selectedHDRI = 0;
#pragma once

#include "platform.h"
#include "Structs.h"
#include "BufferManagement.h"
#include "ModelLoader.h"

#include <glad/glad.h>


void Init(App* app);

void Gui(App* app);

void Update(App* app);

void Render(App* app);

void CleanUp(App* app);

GLuint FindVAO(Mesh& mesh, u32 submeshIndex, const Program& program);

u32 LoadTexture2D(App* app, const char* filepath);



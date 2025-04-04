//
// engine.cpp : Put all your graphics stuff in this file. This is kind of the graphics module.
// In here, you should type all your OpenGL commands, and you can also type code to handle
// input platform events (e.g to move the camera or react to certain shortcuts), writing some
// graphics related GUI options, and so on.
//

#include "engine.h"
#include "ModelLoader.h"

#include <imgui.h>
#include <stb_image.h>
#include <stb_image_write.h>

glm::mat4 TransformScale(const vec3& scaleFactors)
{
    glm::mat4 transform = scale(scaleFactors);
    return transform;
}

glm::mat4 TransformPositionScale(const vec3& pos, const vec3& scaleFactor)
{
    glm::mat4 transform = translate(pos);
    transform = scale(transform, scaleFactor);
    return transform;
}

void CreateEntity(App* app, const u32 aModelIdx, const glm::mat4& aVP, const glm::vec3& aPos)
{
    Entity entity;

    AlignHead(app->entityUBO, app->uniformBlockAligment);
    entity.entityBufferOffset = app->entityUBO.head;

    entity.worldMatrix = glm::translate(aPos);
    entity.modelIndex = aModelIdx;

    PushMat4(app->entityUBO, entity.worldMatrix);
    PushMat4(app->entityUBO, aVP * entity.worldMatrix);

    entity.entityBufferSize = app->entityUBO.head - entity.entityBufferOffset;

    app->entities.push_back(entity);
}

GLuint CreateProgramFromSource(String programSource, const char* shaderName)
{
    GLchar  infoLogBuffer[1024] = {};
    GLsizei infoLogBufferSize = sizeof(infoLogBuffer);
    GLsizei infoLogSize;
    GLint   success;

    char versionString[] = "#version 460\n";
    char shaderNameDefine[128];
    sprintf(shaderNameDefine, "#define %s\n", shaderName);
    char vertexShaderDefine[] = "#define VERTEX\n";
    char fragmentShaderDefine[] = "#define FRAGMENT\n";

    const GLchar* vertexShaderSource[] = {
        versionString,
        shaderNameDefine,
        vertexShaderDefine,
        programSource.str
    };
    const GLint vertexShaderLengths[] = {
        (GLint)strlen(versionString),
        (GLint)strlen(shaderNameDefine),
        (GLint)strlen(vertexShaderDefine),
        (GLint)programSource.len
    };
    const GLchar* fragmentShaderSource[] = {
        versionString,
        shaderNameDefine,
        fragmentShaderDefine,
        programSource.str
    };
    const GLint fragmentShaderLengths[] = {
        (GLint)strlen(versionString),
        (GLint)strlen(shaderNameDefine),
        (GLint)strlen(fragmentShaderDefine),
        (GLint)programSource.len
    };

    GLuint vshader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vshader, ARRAY_COUNT(vertexShaderSource), vertexShaderSource, vertexShaderLengths);
    glCompileShader(vshader);
    glGetShaderiv(vshader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vshader, infoLogBufferSize, &infoLogSize, infoLogBuffer);
        ELOG("glCompileShader() failed with vertex shader %s\nReported message:\n%s\n", shaderName, infoLogBuffer);
    }

    GLuint fshader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fshader, ARRAY_COUNT(fragmentShaderSource), fragmentShaderSource, fragmentShaderLengths);
    glCompileShader(fshader);
    glGetShaderiv(fshader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fshader, infoLogBufferSize, &infoLogSize, infoLogBuffer);
        ELOG("glCompileShader() failed with fragment shader %s\nReported message:\n%s\n", shaderName, infoLogBuffer);
    }

    GLuint programHandle = glCreateProgram();
    glAttachShader(programHandle, vshader);
    glAttachShader(programHandle, fshader);
    glLinkProgram(programHandle);
    glGetProgramiv(programHandle, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(programHandle, infoLogBufferSize, &infoLogSize, infoLogBuffer);
        ELOG("glLinkProgram() failed with program %s\nReported message:\n%s\n", shaderName, infoLogBuffer);
    }

    glUseProgram(0);

    glDetachShader(programHandle, vshader);
    glDetachShader(programHandle, fshader);
    glDeleteShader(vshader);
    glDeleteShader(fshader);

    return programHandle;
}

u32 LoadProgram(App* app, const char* filepath, const char* programName)
{
    String programSource = ReadTextFile(filepath);

    Program program = {};
    program.handle = CreateProgramFromSource(programSource, programName);
    program.filepath = filepath;
    program.programName = programName;
    program.lastWriteTimestamp = GetFileLastWriteTimestamp(filepath);

    if (program.handle != 0)
    {
        GLint AttributeCount = 0UL;
        glGetProgramiv(program.handle, GL_ACTIVE_ATTRIBUTES, &AttributeCount);
        for (size_t i = 0; i < AttributeCount; ++i)
        {
            GLchar Name[248];
            GLsizei realNameSize = 0UL;
            GLsizei attribSize = 0UL;
            GLenum attribType;
            glGetActiveAttrib(program.handle, i, ARRAY_COUNT(Name), &realNameSize, &attribSize, &attribType, Name);
            GLuint attribLocation = glGetAttribLocation(program.handle, Name);
            program.vertexInputLayout.attributes.push_back({ static_cast<u8>(attribLocation), static_cast<u8>(attribSize) });
        }
    }

    app->programs.push_back(program);

    return app->programs.size() - 1;
}

Image LoadImage(const char* filename)
{
    Image img = {};
    stbi_set_flip_vertically_on_load(true);
    img.pixels = stbi_load(filename, &img.size.x, &img.size.y, &img.nchannels, 0);
    if (img.pixels)
    {
        img.stride = img.size.x * img.nchannels;
    }
    else
    {
        ELOG("Could not open file %s", filename);
    }
    return img;
}

GLuint CreateTexture2DFromImage(Image image)
{
    GLenum internalFormat = GL_RGB8;
    GLenum dataFormat = GL_RGB;
    GLenum dataType = GL_UNSIGNED_BYTE;

    switch (image.nchannels)
    {
    case 3: dataFormat = GL_RGB; internalFormat = GL_RGB8; break;
    case 4: dataFormat = GL_RGBA; internalFormat = GL_RGBA8; break;
    default: ELOG("LoadTexture2D() - Unsupported number of channels");
    }

    GLuint texHandle;
    glGenTextures(1, &texHandle);
    glBindTexture(GL_TEXTURE_2D, texHandle);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, image.size.x, image.size.y, 0, dataFormat, dataType, image.pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    return texHandle;
}

void FreeImage(Image image)
{
    stbi_image_free(image.pixels);
}

u32 LoadTexture2D(App* app, const char* filepath)
{
    for (u32 texIdx = 0; texIdx < app->textures.size(); ++texIdx)
        if (app->textures[texIdx].filepath == filepath)
            return texIdx;

    Image image = LoadImage(filepath);

    if (image.pixels)
    {
        Texture tex = {};
        tex.handle = CreateTexture2DFromImage(image);
        tex.filepath = filepath;

        u32 texIdx = app->textures.size();
        app->textures.push_back(tex);

        FreeImage(image);
        return texIdx;
    }
    else
    {
        return UINT32_MAX;
    }
}

void RenderScreenFillQuad(App* app, const FrameBuffer& aFBO)
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(0.0, 0.0, 0.0, 0.0);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glViewport(0, 0, app->displaySize.x, app->displaySize.y);

    Program& programTexturedGeometry = app->programs[app->texturedGeometryProgramIdx];
    glUseProgram(programTexturedGeometry.handle);

    glBindVertexArray(app->vao);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindBufferRange(GL_UNIFORM_BUFFER, 0, app->globalUBO.handle, 0, app->globalUBO.size);

    size_t iteration = 0;
    const char* uniformNames[] = { "uAlbedo", "uNormals", "uPosition", "uViewDir" };
    for (const auto& texture : aFBO.attachments)
    {
        GLint uniformPosition = glGetUniformLocation(programTexturedGeometry.handle, uniformNames[iteration]);
        
        //glUniform1i(uniformPosition, iteration);
        glActiveTexture(GL_TEXTURE0 + iteration);
        glBindTexture(GL_TEXTURE_2D, texture.second);
        ++iteration;
    }

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

    glBindVertexArray(0);
    glUseProgram(0);
}

void Init(App* app)
{
    // TODO: Initialize your resources here!
    glEnable(GL_DEPTH_TEST);

    app->quadVertexBuffer = CreateStaticIndexBuffer(sizeof(vertices));

    glBindBuffer(GL_ARRAY_BUFFER, app->quadVertexBuffer.handle);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glGenBuffers(1, &app->embeddedElements);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, app->embeddedElements);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glGenVertexArrays(1, &app->vao);
    glBindVertexArray(app->vao);
    
    glBindBuffer(GL_ARRAY_BUFFER, app->quadVertexBuffer.handle);   
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexV3V2), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(VertexV3V2), (void*)12);
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, app->embeddedElements);

    glBindVertexArray(0);


    //Geometry Rendering loads
    app->texturedGeometryProgramIdx = LoadProgram(app, "RENDER_QUAD.glsl", "RENDER_QUAD_DEFERRED");
    app->programUniformTexture = glGetUniformLocation(app->programs[app->texturedGeometryProgramIdx].handle, "uTexture");

    app->diceTexIdx = LoadTexture2D(app, "dice.png");
    app->whiteTexIdx = LoadTexture2D(app, "color_white.png");
    app->blackTexIdx = LoadTexture2D(app, "color_black.png");
    app->normalTexIdx = LoadTexture2D(app, "color_normal.png");
    app->magentaTexIdx = LoadTexture2D(app, "color_magenta.png");

    //Geometry Rendering loads

    app->ModelIdx = LoadModel(app, "Queen/Queen.obj");
    u32 planeIdx = LoadModel(app, "Plane/Plane.obj");

    app->geometryProgramIdx = LoadProgram(app, "RENDER_GEOMETRY.glsl", "RENDER_GEOMETRY_DEFERRED");
    app->ModelTextureUniform = glGetUniformLocation(app->programs[app->geometryProgramIdx].handle, "uTexture");

    //Class06
  
    float aspectRatio = (float)app->displaySize.x / (float)app->displaySize.y;
    float near = 0.1f;
    float far = 1000.0f;
    app->worldCamera.ProjectionMatrix = glm::perspective(glm::radians(60.f), aspectRatio, near, far);
    app->worldCamera.Position = vec3(0, 7, 20);
    app->worldCamera.ViewMatrix = glm::lookAt(app->worldCamera.Position, vec3(0, 2, 0), vec3(0, 1, 0));
    
    glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &app->maxUniformBufferSize);
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &app->uniformBlockAligment);

    app->globalUBO = CreateConstantBuffer(app->maxUniformBufferSize);
    app->entityUBO = CreateConstantBuffer(app->maxUniformBufferSize);

    //Lights
    app->lights.push_back({ LightType::Light_Directional, vec3(0.5), vec3(-1,-1,1),vec3(0)});
    app->lights.push_back({ LightType::Light_Point, vec3(1,0.6,0.6), vec3(0),vec3(-4,8,0) });
    app->lights.push_back({ LightType::Light_Point, vec3(1,0.8,0.6), vec3(0),vec3(5,1,0) });

    UpdateLights(app);

    //Entities Buffer
    Buffer& entityUBO = app->entityUBO;
    MapBuffer(entityUBO, GL_WRITE_ONLY);
    glm::mat4 VP = app->worldCamera.ProjectionMatrix * app->worldCamera.ViewMatrix;

    //Geometry Plane
    CreateEntity(app, planeIdx, VP, vec3(0));

    //Geometry Queen
    std::vector<glm::vec3> positions = {
     {0, 0, 1},   // Entidad central al frente
     {-3, 0, -3}, // Entidad izquierda atrás
     {3, 0, -3}   // Entidad derecha atrás
    };

    for (const auto& pos : positions)
    {
        CreateEntity(app, app->ModelIdx, VP, pos);

    }
    UnmapBuffer(entityUBO);

    app->mode = Mode_Deferred_Geometry;
    
    if (!app->primaryFBO.CreateFBO(4, app->displaySize.x, app->displaySize.y)) {
        ELOG("Failed to create FBO");
        return;
    }

}

void Gui(App* app)
{

    ImGui::Begin("Unhooked.v3 Parameters");

        ImGui::Text("FPS: %f", 1.0f / app->deltaTime);
        //displaySize = ImGui::GetContentRegionAvail();;

        if (ImGui::CollapsingHeader("OpenGL Info"))
        {
            ImGui::TextWrapped("%s", app->mOpenGLInfo.c_str());
        }

        ImGui::Separator();

        bool lightChanged = false;
        if (ImGui::CollapsingHeader("Lights"))
        {

            for (auto& light : app->lights)
            {
                vec3 checkVector;
                ImGui::PushID(&light);
                float color[3] = { light.color.x, light.color.y, light.color.z };
                ImGui::DragFloat3("Color", color, 0.01, 0.0, 1.0);
                checkVector = vec3(color[0], color[1], color[2]);
                if (checkVector != light.color)
                {
                    light.color = checkVector;
                    lightChanged = true;
                }

                float direction[3] = { light.direction.x, light.direction.y, light.direction.z };
                ImGui::DragFloat3("Direction", direction, 0.01, -1.0, 1.0);
                checkVector = vec3(direction[0], direction[1], direction[2]);
                if (checkVector != light.direction)
                {
                    light.direction = checkVector;
                    lightChanged = true;
                }

                float position[3] = { light.position.x, light.position.y, light.position.z };
                ImGui::DragFloat3("Position", position);
                checkVector = vec3(position[0], position[1], position[2]);
                if (checkVector != light.position)
                {
                    light.position = checkVector;
                    lightChanged = true;
                }
                ImGui::PopID();
                ImGui::Separator();

                if (lightChanged)
                {
                    UpdateLights(app);
                }
            }
        }
    

    ImGui::End();
}

void Update(App* app)
{
    // You can handle app->input keyboard/mouse here
}

void UpdateLights(App* app)
{
    MapBuffer(app->globalUBO, GL_WRITE_ONLY);
    PushVec3(app->globalUBO, app->worldCamera.Position);
    PushUInt(app->globalUBO, app->lights.size());
    for (size_t i = 0; i < app->lights.size(); i++)
    {
        AlignHead(app->globalUBO, sizeof(vec4));
        Light& light = app->lights[i];
        PushUInt(app->globalUBO, static_cast<unsigned int>(light.type));
        PushVec3(app->globalUBO, light.color);
        PushVec3(app->globalUBO, light.direction);
        PushVec3(app->globalUBO, light.position);

    }
    UnmapBuffer(app->globalUBO);
}

void Render(App* app)
{
    switch (app->mode)
    {
        case Mode_Textured_Geometry:
        {

            glClearColor(0.0, 0.0, 0.0, 0.0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glViewport(0, 0, app->displaySize.x, app->displaySize.y);

            Program& programTexturedGeometry = app->programs[app->texturedGeometryProgramIdx];
            glUseProgram(programTexturedGeometry.handle);

            glBindVertexArray(app->vao);

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glUniform1i(app->programUniformTexture, 0);
            glActiveTexture(GL_TEXTURE0);
            GLuint textureHandle = app->textures[app->diceTexIdx].handle;
            glBindTexture(GL_TEXTURE_2D, textureHandle);

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

            glBindVertexArray(0);
            glUseProgram(0);

            
        }
        break;

        case Mode_Forward_Geometry:
        {
            glClearColor(0.0, 0.0, 0.0, 0.0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glViewport(0, 0, app->displaySize.x, app->displaySize.y);

            Program& geometryProgram = app->programs[app->geometryProgramIdx];
            glUseProgram(geometryProgram.handle);

            glBindBufferRange(GL_UNIFORM_BUFFER, 0, app->globalUBO.handle, 0, app->globalUBO.size);

            for (const auto& entity : app->entities)
            {
                glBindBufferRange(GL_UNIFORM_BUFFER, 1, app->entityUBO.handle, entity.entityBufferOffset, entity.entityBufferSize);
                Model& model = app->models[entity.modelIndex];
                Mesh& mesh = app->meshes[model.meshIdx];

                for (size_t i = 0; i < mesh.submeshes.size(); ++i)
                {
                    GLuint vao = FindVAO(mesh, i, geometryProgram);
                    glBindVertexArray(vao);

                    u32 submeshMaterialIdx = model.materialIdx[i];
                    Material& submeshMaterial = app->materials[submeshMaterialIdx];

                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, app->textures[submeshMaterial.albedoTextureIdx].handle);

                    glUniform1i(app->ModelTextureUniform, 0);

                    SubMesh& submesh = mesh.submeshes[i];
                    glDrawElements(GL_TRIANGLES, submesh.indices.size(), GL_UNSIGNED_INT, (void*)(u64)submesh.indexOffset);

                    glBindTexture(GL_TEXTURE_2D, 0);
                }
            }

            
        }
        break;

        case Mode_Deferred_Geometry:
        {
            glClearColor(0.0, 0.0, 0.0, 0.0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            
            glBindFramebuffer(GL_FRAMEBUFFER, app->primaryFBO.handle);
            std::vector<GLuint> textures;
            for (auto& it : app->primaryFBO.attachments)
            {
                textures.push_back(it.second);
            }
            glDrawBuffers(textures.size(), textures.data());

            glClearColor(0.0, 0.0, 0.0, 0.0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glViewport(0, 0, app->displaySize.x, app->displaySize.y);

            Program& geometryProgram = app->programs[app->geometryProgramIdx];
            glUseProgram(geometryProgram.handle);

            glBindBufferRange(GL_UNIFORM_BUFFER, 0, app->globalUBO.handle, 0, app->globalUBO.size);

            for (const auto& entity : app->entities)
            {
                glBindBufferRange(GL_UNIFORM_BUFFER, 1, app->entityUBO.handle, entity.entityBufferOffset, entity.entityBufferSize);
                Model& model = app->models[entity.modelIndex];
                Mesh& mesh = app->meshes[model.meshIdx];

                for (size_t i = 0; i < mesh.submeshes.size(); ++i)
                {
                    GLuint vao = FindVAO(mesh, i, geometryProgram);
                    glBindVertexArray(vao);

                    u32 submeshMaterialIdx = model.materialIdx[i];
                    Material& submeshMaterial = app->materials[submeshMaterialIdx];

                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, app->textures[submeshMaterial.albedoTextureIdx].handle);

                    glUniform1i(app->ModelTextureUniform, 0);

                    SubMesh& submesh = mesh.submeshes[i];
                    glDrawElements(GL_TRIANGLES, submesh.indices.size(), GL_UNSIGNED_INT, (void*)(u64)submesh.indexOffset);

                    glBindTexture(GL_TEXTURE_2D, 0);
                }
            }
            glUseProgram(0);
            glBindFramebuffer(GL_FRAMEBUFFER,0);

            RenderScreenFillQuad(app, app->primaryFBO);
           
        }
        break;

      default:;
    }
}

void CleanUp(App* app)
{
    ELOG("Cleaning up engine");

    for (auto& texture : app->textures)
    {
        glDeleteTextures(1, &texture.handle);
    };

    for (auto& program : app->programs)
    {
        program.handle = 0;
    };


    if (app->vao != 0)
    {
        glDeleteVertexArrays(1, &app->vao);
        app->vao = 0;
    }
    if (app->quadVertexBuffer.handle != 0)
    {
        glDeleteBuffers(1, &app->quadVertexBuffer.handle);
        app->quadVertexBuffer.handle = 0;
    }
    if (app->embeddedElements != 0)
    {
        glDeleteBuffers(1, &app->embeddedElements);
        app->embeddedElements = 0;
    }


}

GLuint FindVAO(Mesh& mesh, u32 submeshIndex, const Program& program)
{
    SubMesh& submesh = mesh.submeshes[submeshIndex];

    // Try finding a vao for this submesh/program
    for (u32 i = 0; i < submesh.vaos.size(); ++i) {
        if (submesh.vaos[i].programHandle == program.handle) {
            return submesh.vaos[i].handle;
        }
    }

    // Create a new vao for this submesh/program
    GLuint vaoHandle;


    glGenVertexArrays(1, &vaoHandle);
    glBindVertexArray(vaoHandle);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vertexBufferHandle);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indexBufferHandle);

    // We have to link all vertex inputs attributes to attributes in the vertex buffer
    for (auto& shaderLayout : program.vertexInputLayout.attributes)
    {
        bool attributeWasLinked = false;

        for (auto& meshLayout : submesh.vertexBufferLayout.attributes)
        {
            if (shaderLayout.location == meshLayout.location)
            {
                const u32 index = meshLayout.location;
                const u32 ncomp = meshLayout.componentCount;
                const u32 offset = meshLayout.offset + submesh.vertexOffset; // attribute offset + vertex offset
                const u32 stride = submesh.vertexBufferLayout.stride;
                glVertexAttribPointer(index, ncomp, GL_FLOAT, GL_FALSE, stride, (void*)(u64)offset);
                glEnableVertexAttribArray(index);

                attributeWasLinked = true;
                break;
            }
        }

        assert(attributeWasLinked); // The submesh should provide an attribute for each vertex inputs
    }

    glBindVertexArray(0);

    // Almacenar el nuevo VAO en la lista para este submesh
    Vao vao = { vaoHandle, program.handle };
    submesh.vaos.push_back(vao);

    return vaoHandle;
}



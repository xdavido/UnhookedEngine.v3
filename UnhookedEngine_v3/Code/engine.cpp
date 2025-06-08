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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

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

    glUniform1i(glGetUniformLocation(programTexturedGeometry.handle, "uDisplayMode"), static_cast<int>(app->deferredDisplayMode));
    glUniform1i(glGetUniformLocation(programTexturedGeometry.handle, "uInvertDepth"), static_cast<int>(app->InverseDepth));

    size_t iteration = 0;
    const char* uniformNames[] = { "uAlbedo", "uNormals", "uPosition", "uViewDir" };
    for (const auto& texture : aFBO.attachments)
    {
        GLint uniformLocation = glGetUniformLocation(programTexturedGeometry.handle, uniformNames[iteration]);
        glActiveTexture(GL_TEXTURE0 + iteration);
        glBindTexture(GL_TEXTURE_2D, texture.second);
        glUniform1i(uniformLocation, iteration);
        ++iteration;
    }

    // Depth texture
    GLint depthLoc = glGetUniformLocation(programTexturedGeometry.handle, "uDepth");
    glActiveTexture(GL_TEXTURE0 + iteration);
    glBindTexture(GL_TEXTURE_2D, aFBO.depthHandle);
    glUniform1i(depthLoc, iteration);
    ++iteration;

    // ✅ Skybox cubemap
    glActiveTexture(GL_TEXTURE0 + iteration);
    glBindTexture(GL_TEXTURE_CUBE_MAP, app->skyboxMap);
    glUniform1i(glGetUniformLocation(programTexturedGeometry.handle, "skybox"), iteration);
    ++iteration;

    // ✅ Viewport size
    glUniform2f(glGetUniformLocation(programTexturedGeometry.handle, "viewportSize"), (float)app->displaySize.x, (float)app->displaySize.y);

    // ✅ Matrices inversas
    glm::mat4 invProj = glm::inverse(app->worldCamera.ProjectionMatrix);
    glm::mat4 invView = glm::inverse(app->worldCamera.ViewMatrix);

    glUniformMatrix4fv(glGetUniformLocation(programTexturedGeometry.handle, "inverseProjection"), 1, GL_FALSE, glm::value_ptr(invProj));
    glUniformMatrix4fv(glGetUniformLocation(programTexturedGeometry.handle, "inverseView"), 1, GL_FALSE, glm::value_ptr(invView));

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

    glBindVertexArray(0);
    glUseProgram(0);
}


u32 ConvertHDRIToCubemap(App* app, const char* filepath)
{
    // 1. Cargar imagen HDR
    Image hdr = {};
    stbi_set_flip_vertically_on_load(true);
    hdr.pixels = stbi_loadf(filepath, &hdr.size.x, &hdr.size.y, &hdr.nchannels, 0);
    if (!hdr.pixels) {
        ELOG("No se pudo cargar HDRI %s", filepath);
        return 0;
    }

    GLuint hdrTexture;
    glGenTextures(1, &hdrTexture);
    glBindTexture(GL_TEXTURE_2D, hdrTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, hdr.size.x, hdr.size.y, 0, GL_RGB, GL_FLOAT, hdr.pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    FreeImage(hdr);

    // 2. Crear cubemap vacío
    GLuint cubemapTex;
    glGenTextures(1, &cubemapTex);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTex);
    for (int i = 0; i < 6; ++i)
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 512, 512, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    // 3. Cargar shader de conversión
    u32 progIdx = LoadProgram(app, "EQUIRECT_TO_CUBEMAP.glsl", "EQUIRECT_TO_CUBEMAP");
    Program& program = app->programs[progIdx];
    glUseProgram(program.handle);

    glUniform1i(glGetUniformLocation(program.handle, "equirectangularMap"), 0);

    // 4. Matrices de vista (una por cara del cubo)
    glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.f, 0.1f, 10.0f);
    glm::mat4 captureViews[] = {
        glm::lookAt(glm::vec3(0), glm::vec3(1, 0, 0), glm::vec3(0, -1, 0)),
        glm::lookAt(glm::vec3(0), glm::vec3(-1, 0, 0), glm::vec3(0, -1, 0)),
        glm::lookAt(glm::vec3(0), glm::vec3(0, 1, 0), glm::vec3(0, 0, 1)),
        glm::lookAt(glm::vec3(0), glm::vec3(0, -1, 0), glm::vec3(0, 0, -1)),
        glm::lookAt(glm::vec3(0), glm::vec3(0, 0, 1), glm::vec3(0, -1, 0)),
        glm::lookAt(glm::vec3(0), glm::vec3(0, 0, -1), glm::vec3(0, -1, 0))
    };

    // 5. FBO
    GLuint fbo, rbo;
    glGenFramebuffers(1, &fbo);
    glGenRenderbuffers(1, &rbo);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);

    // 6. Dibujar cubo en cada cara
    glViewport(0, 0, 512, 512);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdrTexture);

    GLuint cubeVAO = CreateCubeVAO(); // Necesitas una función que cree un cubo simple

    for (unsigned int i = 0; i < 6; ++i)
    {
        glUniformMatrix4fv(glGetUniformLocation(program.handle, "view"), 1, GL_FALSE, glm::value_ptr(captureViews[i]));
        glUniformMatrix4fv(glGetUniformLocation(program.handle, "projection"), 1, GL_FALSE, glm::value_ptr(captureProjection));
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cubemapTex, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glBindVertexArray(cubeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    //glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTex);
    //glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteRenderbuffers(1, &rbo);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &hdrTexture);
    glUseProgram(0);

    return cubemapTex;
}

void CreateIrradianceMap(App* app)
{
    const u32 IRRADIANCE_RES = 32;

    // Crear textura cubemap para irradiancia
    glGenTextures(1, &app->irradianceMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, app->irradianceMap);
    for (u32 i = 0; i < 6; ++i)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F,
            IRRADIANCE_RES, IRRADIANCE_RES, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Setup FBO temporal para convolución
    GLuint fbo, rbo;
    glGenFramebuffers(1, &fbo);
    glGenRenderbuffers(1, &rbo);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, IRRADIANCE_RES, IRRADIANCE_RES);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);

    // Shader de convolución
    Program& irradianceShader = app->programs[app->irradianceConvolutionProgramIdx];
    glUseProgram(irradianceShader.handle);
    glUniform1i(glGetUniformLocation(irradianceShader.handle, "environmentMap"), 0);

    glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    glUniformMatrix4fv(glGetUniformLocation(irradianceShader.handle, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    static glm::mat4 views[] = {
        glm::lookAt(glm::vec3(0, 0, 0), glm::vec3(1, 0, 0), glm::vec3(0, -1, 0)),
        glm::lookAt(glm::vec3(0, 0, 0), glm::vec3(-1, 0, 0), glm::vec3(0, -1, 0)),
        glm::lookAt(glm::vec3(0, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, 0, 1)),
        glm::lookAt(glm::vec3(0, 0, 0), glm::vec3(0, -1, 0), glm::vec3(0, 0, -1)),
        glm::lookAt(glm::vec3(0, 0, 0), glm::vec3(0, 0, 1), glm::vec3(0, -1, 0)),
        glm::lookAt(glm::vec3(0, 0, 0), glm::vec3(0, 0, -1), glm::vec3(0, -1, 0))
    };

    glViewport(0, 0, IRRADIANCE_RES, IRRADIANCE_RES);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, app->skyboxMap);

    for (u32 i = 0; i < 6; ++i)
    {
        glUniformMatrix4fv(glGetUniformLocation(irradianceShader.handle, "view"), 1, GL_FALSE, glm::value_ptr(views[i]));

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, app->irradianceMap, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        RenderCube(); // Tu VAO de cubo unitario
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteRenderbuffers(1, &rbo);
}

void CreatePrefilteredMap(App* app)
{
    const u32 PREFILTERED_RES = 128;
    const u32 MAX_MIP_LEVELS = 5;

    glGenTextures(1, &app->prefilteredMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, app->prefilteredMap);

    for (u32 mip = 0; mip < MAX_MIP_LEVELS; ++mip)
    {
        u32 mipWidth = PREFILTERED_RES >> mip;
        u32 mipHeight = PREFILTERED_RES >> mip;
        for (u32 face = 0; face < 6; ++face)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, mip, GL_RGB16F, mipWidth, mipHeight, 0, GL_RGB, GL_FLOAT, nullptr);
        }
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    GLuint fbo, rbo;
    glGenFramebuffers(1, &fbo);
    glGenRenderbuffers(1, &rbo);

    Program& prefilterShader = app->programs[app->prefilterConvolutionProgramIdx];
    glUseProgram(prefilterShader.handle);
    glUniform1i(glGetUniformLocation(prefilterShader.handle, "environmentMap"), 0);

    glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.f, 0.1f, 10.0f);
    glUniformMatrix4fv(glGetUniformLocation(prefilterShader.handle, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    static glm::mat4 views[] = {
        glm::lookAt(glm::vec3(0), glm::vec3(1, 0, 0), glm::vec3(0, -1, 0)),
        glm::lookAt(glm::vec3(0), glm::vec3(-1, 0, 0), glm::vec3(0, -1, 0)),
        glm::lookAt(glm::vec3(0), glm::vec3(0, 1, 0), glm::vec3(0, 0, 1)),
        glm::lookAt(glm::vec3(0), glm::vec3(0, -1, 0), glm::vec3(0, 0, -1)),
        glm::lookAt(glm::vec3(0), glm::vec3(0, 0, 1), glm::vec3(0, -1, 0)),
        glm::lookAt(glm::vec3(0), glm::vec3(0, 0, -1), glm::vec3(0, -1, 0))
    };

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, app->skyboxMap);

    for (u32 mip = 0; mip < MAX_MIP_LEVELS; ++mip)
    {
        u32 mipSize = PREFILTERED_RES >> mip;
        glBindRenderbuffer(GL_RENDERBUFFER, rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipSize, mipSize);
        glViewport(0, 0, mipSize, mipSize);

        float roughness = (float)mip / (float)(MAX_MIP_LEVELS - 1);
        glUniform1f(glGetUniformLocation(prefilterShader.handle, "roughness"), roughness);

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            ELOG("Framebuffer incompleto en mip %u!", mip);
        }

        for (u32 i = 0; i < 6; ++i)
        {
            glUniformMatrix4fv(glGetUniformLocation(prefilterShader.handle, "view"), 1, GL_FALSE, glm::value_ptr(views[i]));
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, app->prefilteredMap, mip);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            RenderCube();
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteRenderbuffers(1, &rbo);
}

GLuint CreateCubeVAO()
{
    float cubeVertices[] = {
        // pos
        -1, -1, -1,  1, -1, -1,  1,  1, -1,
        -1, -1, -1,  1,  1, -1, -1,  1, -1,
        -1, -1,  1,  1, -1,  1,  1,  1,  1,
        -1, -1,  1,  1,  1,  1, -1,  1,  1,
        -1,  1,  1,  1,  1,  1,  1,  1, -1,
        -1,  1,  1,  1,  1, -1, -1,  1, -1,
        -1, -1,  1,  1, -1,  1,  1, -1, -1,
        -1, -1,  1,  1, -1, -1, -1, -1, -1,
         1, -1,  1,  1,  1,  1,  1,  1, -1,
         1, -1,  1,  1,  1, -1,  1, -1, -1,
        -1, -1,  1, -1,  1,  1, -1,  1, -1,
        -1, -1,  1, -1,  1, -1, -1, -1, -1
    };

    GLuint cubeVAO, cubeVBO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);

    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);

    return cubeVAO;
}

void CreateSphereVAO(GLuint& vao, GLuint& vbo, GLuint& ibo, u32& indexCount, int sectorCount = 64, int stackCount = 32)
{
    std::vector<float> vertices;
    std::vector<u32> indices;


    for (int i = 0; i <= stackCount; ++i)
    {
        float stackAngle = PI / 2 - i * PI / stackCount;  // from pi/2 to -pi/2
        float xy = cos(stackAngle);
        float z = sin(stackAngle);

        for (int j = 0; j <= sectorCount; ++j)
        {
            float sectorAngle = j * 2 * PI / sectorCount;

            float x = xy * cos(sectorAngle);
            float y = xy * sin(sectorAngle);

            vertices.push_back(x); vertices.push_back(y); vertices.push_back(z);   // Position
            vertices.push_back(x); vertices.push_back(y); vertices.push_back(z);   // Normal
        }
    }

    for (int i = 0; i < stackCount; ++i)
    {
        int k1 = i * (sectorCount + 1);
        int k2 = k1 + sectorCount + 1;

        for (int j = 0; j < sectorCount; ++j, ++k1, ++k2)
        {
            if (i != 0) {
                indices.push_back(k1);
                indices.push_back(k2);
                indices.push_back(k1 + 1);
            }
            if (i != (stackCount - 1)) {
                indices.push_back(k1 + 1);
                indices.push_back(k2);
                indices.push_back(k2 + 1);
            }
        }
    }

    indexCount = static_cast<u32>(indices.size());

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ibo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(u32), indices.data(), GL_STATIC_DRAW);

    // Position attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);

    // Normal attribute
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));

    glBindVertexArray(0);
}


void CreateCubeVAO(GLuint& vao, GLuint& vbo, GLuint& ibo, u32& indexCount)
{
    float vertices[] = {
        // pos               // normals
        -1, -1, -1,   0,  0, -1,
         1, -1, -1,   0,  0, -1,
         1,  1, -1,   0,  0, -1,
        -1,  1, -1,   0,  0, -1,

        -1, -1,  1,   0,  0, 1,
         1, -1,  1,   0,  0, 1,
         1,  1,  1,   0,  0, 1,
        -1,  1,  1,   0,  0, 1,

        -1, -1, -1,  -1,  0, 0,
        -1,  1, -1,  -1,  0, 0,
        -1,  1,  1,  -1,  0, 0,
        -1, -1,  1,  -1,  0, 0,

         1, -1, -1,   1,  0, 0,
         1,  1, -1,   1,  0, 0,
         1,  1,  1,   1,  0, 0,
         1, -1,  1,   1,  0, 0,

        -1, -1, -1,   0, -1, 0,
        -1, -1,  1,   0, -1, 0,
         1, -1,  1,   0, -1, 0,
         1, -1, -1,   0, -1, 0,

        -1,  1, -1,   0, 1, 0,
        -1,  1,  1,   0, 1, 0,
         1,  1,  1,   0, 1, 0,
         1,  1, -1,   0, 1, 0,
    };

    u32 indices[] = {
         0,  1,  2,  2,  3,  0,
         4,  5,  6,  6,  7,  4,
         8,  9, 10, 10, 11,  8,
        12, 13, 14, 14, 15, 12,
        16, 17, 18, 18, 19, 16,
        20, 21, 22, 22, 23, 20
    };
    indexCount = sizeof(indices) / sizeof(u32);

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ibo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // Position (location = 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)0);

    // Normal (location = 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 3));

    glBindVertexArray(0);
}

void Init(App* app)
{
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

    app->waterModelIdx = LoadModel(app, "Water/Water.obj");

    app->dudvMap = LoadTexture2D(app, "Water/dudvmap2.jpg");
    app->normalMap = LoadTexture2D(app, "Water/normalmap2.jpg");

    app->foamMap = LoadTexture2D(app, "Water/foamMap.png");
    app->causticsMap = LoadTexture2D(app, "Water/causticsmap.jpg");

    app->skyboxMap = ConvertHDRIToCubemap(app, "Skybox/kloofendal_48d_partly_cloudy_puresky_4k.hdr");
   

    

    

    // Verificar contenido del cubemap
    glBindTexture(GL_TEXTURE_CUBE_MAP, app->skyboxMap);
    GLint w;
    glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_TEXTURE_WIDTH, &w);
    std::cout << "[DEBUG] Cubemap face width: " << w << std::endl;

    
    Model& model = app->models[app->sphereModelIdx];
    Mesh& mesh = app->meshes[model.meshIdx];

    if (mesh.submeshes.empty()) {
        std::cout << "[ERROR] El modelo de esfera no contiene submeshes\n";
    }
    else {
        std::cout << "[DEBUG] Submeshes encontrados: " << mesh.submeshes.size() << std::endl;
    }
    //Scene 1
    app->BaseIdx = LoadModel(app, "Rocks/Base.obj");
    app->SculptIdx = LoadModel(app, "Rocks/Sculpt.obj");

    //Scene 2
    app->SandIdx = LoadModel(app, "Island/Sand.obj");
    app->GrassIdx = LoadModel(app, "Island/Grass.obj");
    app->RocksIdx = LoadModel(app, "Island/Rocks.obj");
    app->ReddoIdx  =LoadModel(app, "Island/Reddo.obj");
    app->MetalIdx  =LoadModel(app, "Island/Metal.obj");
    app->WoodIdx =LoadModel(app, "Island/Wood.obj");
    app->WindowsIdx  =LoadModel(app, "Island/Windows.obj");
    app->StoneIdx  =LoadModel(app, "Island/Stone.obj");

    app->SkyBoxIdx = LoadProgram(app, "SKYBOX.glsl", "SKYBOX");
    app->irradianceConvolutionProgramIdx = LoadProgram(app, "IBL_CONVOLUTION.glsl", "IBL_CONVOLUTION");
    app->irradianceDebugProgramIdx = LoadProgram(app, "IRRADIANCE_DEBUG.glsl", "IRRADIANCE_DEBUG");
    app->prefilterConvolutionProgramIdx = LoadProgram(app, "PREFILTER_CONVOLUTION.glsl", "PREFILTER_CONVOLUTION");

    app->geometryProgramIdx = LoadProgram(app, "RENDER_GEOMETRY.glsl", "RENDER_GEOMETRY_DEFERRED");
    app->ModelTextureUniform = glGetUniformLocation(app->programs[app->geometryProgramIdx].handle, "uTexture");


    app->waterProgramIdx = LoadProgram(app, "WATER_EFFECT.glsl", "WATER_EFFECT_DEFERRED");
    app->refractionProgramIdx = LoadProgram(app, "ENV_REFRACTION.glsl", "ENV_REFRACTION");
    app->reflectionProgramIdx = LoadProgram(app, "ENV_REFLECTION.glsl", "ENV_REFLECTION");
    app->iblCombinedProgramIdx = LoadProgram(app, "IBL_COMBINED.glsl", "IBL_COMBINED");

    CreateCubeVAO(app->vaoRefractionCube, app->vboRefractionCube, app->iboRefractionCube, app->refractionCubeIndexCount);
    CreateSphereVAO(app->vaoSphere, app->vboSphere, app->iboSphere, app->indexCountSphere);
    CreateIrradianceMap(app);
    CreatePrefilteredMap(app);

    app->skyboxMap = app->irradianceMap;

    //Camera
    float aspectRatio = (float)app->displaySize.x / (float)app->displaySize.y;
    
    app->worldCamera.ProjectionMatrix = glm::perspective(glm::radians(60.f), aspectRatio, app->worldCamera.nearPlane, app->worldCamera.farPlane);
    app->worldCamera.Position = vec3(0, 7, 15); //Alejada hacia la derecha
    app->worldCamera.Front = glm::vec3(0.0f, -0.3f, -1.0f); // Apuntando ligeramente hacia abajo
    app->worldCamera.ViewMatrix = glm::lookAt(app->worldCamera.Position, app->worldCamera.Position + app->worldCamera.Front, app->worldCamera.Up);
    
    glm::mat4 VP = app->worldCamera.ProjectionMatrix * app->worldCamera.ViewMatrix;
    
    glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &app->maxUniformBufferSize);
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &app->uniformBlockAligment);

    app->globalUBO = CreateConstantBuffer(app->maxUniformBufferSize);
    app->entityUBO = CreateConstantBuffer(app->maxUniformBufferSize);

    app->currentScene = Scene_Island;


    // Luces para Rocks
    app->lights.push_back({ LightType::Light_Directional, vec3(0.2,0.5,0.9), vec3(-1,-1,1), vec3(0) });
    app->lights.push_back({ LightType::Light_Directional, vec3(0.2,0.09,0.05), vec3(1,0.2,0.8), vec3(0) });
    app->lights.push_back({ LightType::Light_Point, vec3(0.9,0.5,0.05), vec3(-1,-1,1), vec3(2.4,1.1,-1) });
    app->lights.push_back({ LightType::Light_Point, vec3(0.7,0.5,0.15), vec3(-1,-1,1), vec3(0.2,3.5,0.1) });


    MapBuffer(app->entityUBO, GL_WRITE_ONLY);

    app->SkyBoxVao = CreateCubeVAO();

    // Entidades para Rocks
    CreateEntity(app, app->BaseIdx, VP, vec3(0));
    CreateEntity(app, app->SculptIdx, VP, vec3(0));
    CreateEntity(app, app->waterModelIdx, VP, vec3(0));
    

    // Actualizar buffers
    UnmapBuffer(app->entityUBO);
    UpdateLights(app);

    app->mode = Mode_Forward_Geometry;
    app->primaryFBO.CreateFBO(4, app->displaySize.x, app->displaySize.y);

    app->reflectionFBO.CreateFBO(1, app->displaySize.x, app->displaySize.y);
    app->refractionFBO.CreateFBO(1, app->displaySize.x, app->displaySize.y);

}

void Gui(App* app)
{
    ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

    // Estilo general
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 10.0f;
    style.FrameRounding = 6.0f;
    style.FramePadding = ImVec2(10, 6);
    style.ItemSpacing = ImVec2(12, 8);
    style.GrabRounding = 4.0f;

    // Colores personalizados
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.13f, 0.13f, 0.15f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.25f, 0.35f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.35f, 0.50f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.45f, 0.45f, 0.60f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.18f, 0.18f, 0.22f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.35f, 0.35f, 0.50f, 0.7f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.45f, 0.45f, 0.60f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.55f, 0.55f, 0.70f, 1.0f));

    ImGui::Begin(" Unhooked.v3 Parameters");

    ImGui::Text(" FPS: %.1f", 1.0f / app->deltaTime);
    ImGui::Spacing();
    ImGui::Separator();

    if (ImGui::CollapsingHeader(" OpenGL Info", ImGuiTreeNodeFlags_NoAutoOpenOnLog))
    {
        ImGui::TextWrapped("%s", app->mOpenGLInfo.c_str());
    }

    ImGui::Spacing();
    if (ImGui::Button(app->currentScene == Scene_Rocks ? " Switch to Island Scene" : " Switch to Rocks Scene", ImVec2(220, 35)))
    {
        // Limpiar entidades y luces actuales
        app->entities.clear();
        app->lights.clear();

        // Resetear el buffer de entidades
        app->entityUBO.head = 0;
        MapBuffer(app->entityUBO, GL_WRITE_ONLY);

        // Crear nuevas entidades y luces según la escena seleccionada
        glm::mat4 VP = app->worldCamera.ProjectionMatrix * app->worldCamera.ViewMatrix;
        glm::vec3 position = { 0, 0, 0 };

        if (app->currentScene == Scene_Rocks)
        {
            app->skyboxMap = ConvertHDRIToCubemap(app, "Skybox/kloofendal_48d_partly_cloudy_puresky_4k.hdr");
            CreateIrradianceMap(app);
            CreatePrefilteredMap(app);

            // Cambiar a escena Island
            app->currentScene = Scene_Island;

            // Luces para Island
            app->lights.push_back({ LightType::Light_Directional, vec3(1.0), vec3(-1,-1,-1), vec3(0) });
            app->lights.push_back({ LightType::Light_Directional, vec3(1.0,0.8,0.4), vec3(-1,-0.35,0.35), vec3(0) });


            // Entidades para Island
            CreateEntity(app, app->SandIdx, VP, position);
            CreateEntity(app, app->GrassIdx, VP, position);
            CreateEntity(app, app->RocksIdx, VP, position);
            CreateEntity(app, app->ReddoIdx, VP, position);
            CreateEntity(app, app->MetalIdx, VP, position);
            CreateEntity(app, app->WindowsIdx, VP, position);
            CreateEntity(app, app->WoodIdx, VP, position);
            CreateEntity(app, app->StoneIdx, VP, position);
        }
        else
        {
            app->skyboxMap = ConvertHDRIToCubemap(app, "Skybox/kloppenheim_02_puresky_4k.hdr");
            CreateIrradianceMap(app);
            CreatePrefilteredMap(app);

            // Cambiar a escena Rocks
            app->currentScene = Scene_Rocks;

            // Luces para Rocks
            app->lights.push_back({ LightType::Light_Directional, vec3(0.2,0.5,0.9), vec3(-1,-1,1), vec3(0) });
            app->lights.push_back({ LightType::Light_Directional, vec3(0.2,0.09,0.05), vec3(1,0.2,0.8), vec3(0) });
            app->lights.push_back({ LightType::Light_Point, vec3(0.9,0.5,0.05), vec3(-1,-1,1), vec3(2.4,1.1,-1) });
            app->lights.push_back({ LightType::Light_Point, vec3(0.7,0.5,0.15), vec3(-1,-1,1), vec3(0.2,3.5,0.1) });

            // Entidades para Rocks
            CreateEntity(app, app->BaseIdx, VP, position);
            CreateEntity(app, app->SculptIdx, VP, position);
        }

        // Añadir agua a ambas escenas
        CreateEntity(app, app->waterModelIdx, VP, position);

        // Actualizar buffers
        UnmapBuffer(app->entityUBO);
        UpdateLights(app);
    }
    ImGui::Spacing();

    // Modo Rendering
    ImGui::Text(" Render Mode");
    if (ImGui::Button(app->mode == Mode_Forward_Geometry ? " Switch to Deferred" : " Switch to Forward", ImVec2(220, 35)))
    {

        for (auto& program : app->programs)
        {
            if (program.handle != 0)
            {
                glDeleteProgram(program.handle);
                program.handle = 0;
            }
        }
        app->programs.clear();

        app->mode = (app->mode == Mode_Forward_Geometry) ? Mode_Deferred_Geometry : Mode_Forward_Geometry;

        app->texturedGeometryProgramIdx = LoadProgram(app, "RENDER_QUAD.glsl", app->mode == Mode_Deferred_Geometry ? "RENDER_QUAD_DEFERRED" : "RENDER_QUAD_FORWARD");
        app->geometryProgramIdx = LoadProgram(app, "RENDER_GEOMETRY.glsl", app->mode == Mode_Deferred_Geometry ? "RENDER_GEOMETRY_DEFERRED" : "RENDER_GEOMETRY_FORWARD");
        app->waterProgramIdx = LoadProgram(app, "WATER_EFFECT.glsl", app->mode == Mode_Deferred_Geometry ? "WATER_EFFECT_DEFERRED" : "WATER_EFFECT_FORWARD");
        app->iblCombinedProgramIdx = LoadProgram(app, "IBL_COMBINED.glsl", "IBL_COMBINED");
       
        app->SkyBoxIdx = LoadProgram(app, "SKYBOX.glsl", "SKYBOX");
        app->irradianceDebugProgramIdx = LoadProgram(app, "IRRADIANCE_DEBUG.glsl", "IRRADIANCE_DEBUG");
        app->irradianceConvolutionProgramIdx = LoadProgram(app, "IBL_CONVOLUTION.glsl", "IBL_CONVOLUTION");
        app->prefilterConvolutionProgramIdx = LoadProgram(app, "PREFILTER_CONVOLUTION.glsl", "PREFILTER_CONVOLUTION");

            
        //app->refractionProgramIdx = LoadProgram(app, "ENV_REFRACTION.glsl", "ENV_REFRACTION");
        //app->reflectionProgramIdx = LoadProgram(app, "ENV_REFLECTION.glsl", "ENV_REFLECTION");

        app->ModelTextureUniform = glGetUniformLocation(app->programs[app->geometryProgramIdx].handle, "uTexture");
        app->programUniformTexture = glGetUniformLocation(app->programs[app->texturedGeometryProgramIdx].handle, "uTexture");
    }

    ImGui::Spacing();

    // Layout Deferred
    if (app->mode == Mode_Deferred_Geometry)
    {
        ImGui::Text(" Deferred Layout");

        static bool showDeferredModes = false;

        const char* currentMode = "Default";
        switch (app->deferredDisplayMode) {
        case DeferredDisplayMode::Albedo: currentMode = "Albedo"; break;
        case DeferredDisplayMode::Normals: currentMode = "Normals"; break;
        case DeferredDisplayMode::Position: currentMode = "Position"; break;
        case DeferredDisplayMode::ViewDir: currentMode = "ViewDir"; break;
        case DeferredDisplayMode::Depth: currentMode = "Depth"; break;
        default: break;
        }

        if (ImGui::Button(currentMode, ImVec2(200, 40))) {
            showDeferredModes = !showDeferredModes;
        }

        if (app->deferredDisplayMode == DeferredDisplayMode::Depth)
        {
            if (ImGui::SmallButton(app->InverseDepth ? "Invertir OFF" : "Invertir ON"))
            {
                app->InverseDepth = !app->InverseDepth;
            }
        }


        if (showDeferredModes)
        {
            ImGui::BeginChild("DeferredModes", ImVec2(220, 180), true);
            const auto showSelectable = [&](const char* label, DeferredDisplayMode mode)
                {
                    if (ImGui::Selectable(label, app->deferredDisplayMode == mode)) {
                        app->deferredDisplayMode = mode;
                        showDeferredModes = false;
                    }
                };
            showSelectable("Default", DeferredDisplayMode::Default);
            showSelectable("Albedo", DeferredDisplayMode::Albedo);
            showSelectable("Normals", DeferredDisplayMode::Normals);
            showSelectable("Position", DeferredDisplayMode::Position);
            showSelectable("ViewDir", DeferredDisplayMode::ViewDir);
            showSelectable("Depth", DeferredDisplayMode::Depth);
            ImGui::EndChild();
        }
    }

    ImGui::Spacing();
    ImGui::Separator();

    // Stress Test
    static bool stressTestActive = false;
    static std::vector<Light> originalLights;

    if (ImGui::Button(stressTestActive ? " Disable Stress Test (1000 lights)" : " Enable Stress Test (1000 lights)", ImVec2(270, 35)))
    {
        stressTestActive = !stressTestActive;

        if (stressTestActive)
        {
            originalLights = app->lights;
            app->lights.clear();

            const int gridSize = 10;
            const float spacing = 3.0f;
            const float startPos = -(gridSize * spacing) / 2.0f;

            for (int x = 0; x < gridSize; ++x)
                for (int z = 0; z < gridSize; ++z)
                    for (int y = 0; y < 10; ++y)
                        app->lights.push_back({
                            LightType::Light_Point,
                            glm::vec3(rand() / (float)RAND_MAX, rand() / (float)RAND_MAX, rand() / (float)RAND_MAX),
                            glm::vec3(0.0f),
                            glm::vec3(startPos + x * spacing, 0.5f + y * 0.5f, startPos + z * spacing)
                            });
        }
        else
        {
            app->lights = originalLights;
        }
        UpdateLights(app);
    }

    ImGui::Spacing();

    // Lights Panel
    bool lightChanged = false;
    if (ImGui::CollapsingHeader(" Lights"))
    {
        ImGui::Text("Total Lights: %d", app->lights.size());

        for (auto& light : app->lights)
        {
            ImGui::PushID(&light);
            float color[3] = { light.color.x, light.color.y, light.color.z };
            float direction[3] = { light.direction.x, light.direction.y, light.direction.z };
            float position[3] = { light.position.x, light.position.y, light.position.z };

            if (ImGui::ColorEdit3("Color", color)) {
                light.color = glm::vec3(color[0], color[1], color[2]);
                lightChanged = true;
            }

            if (ImGui::DragFloat3("Direction", direction, 0.01f, -1.0f, 1.0f)) {
                light.direction = glm::vec3(direction[0], direction[1], direction[2]);
                lightChanged = true;
            }

            if (ImGui::DragFloat3("Position", position, 0.1f)) {
                light.position = glm::vec3(position[0], position[1], position[2]);
                lightChanged = true;
            }

            ImGui::Separator();
            ImGui::PopID();
        }

        if (lightChanged) UpdateLights(app);
    }

    if (ImGui::CollapsingHeader("Water Effect")) 
    {
        ImGui::SliderFloat("Wave Tile Size", &app->waterTileSize, 0.1f, 20.0f);
    }

    if (ImGui::CollapsingHeader("Camera Orbit"))
    {
        ImGui::Checkbox("Enable Orbit", &app->orbitEnabled);
        ImGui::SliderFloat("Orbit Radius", &app->orbitRadius, 1.0f, 50.0f);
        ImGui::SliderFloat("Orbit Speed", &app->orbitSpeed, 0.1f, 1.0f);
        ImGui::DragFloat3("Orbit Center", &app->orbitCenter[0], 0.1f);
    }

    ImGui::End();

    ImGui::PopStyleColor(8);
}


void UpdateEntities(App* app)
{
    glm::mat4 VP = app->worldCamera.ProjectionMatrix * app->worldCamera.ViewMatrix;
    MapBuffer(app->entityUBO, GL_WRITE_ONLY);

    for (auto& entity : app->entities)
    {
        glm::mat4* vpOffset = (glm::mat4*)((u8*)app->entityUBO.data + entity.entityBufferOffset + sizeof(glm::mat4));
        *vpOffset = VP * entity.worldMatrix;
    }

    UnmapBuffer(app->entityUBO);
}


void Update(App* app)
{
    // Store previous camera position to detect movement
    glm::vec3 prevPosition = app->worldCamera.Position;
    glm::vec3 prevFront = app->worldCamera.Front;

    // Tiempo entre frames para movimiento suave
    float deltaTime = app->deltaTime;

    // Aumentar velocidad si Shift está presionado
    float speedMultiplier = 2.0f;
    if (app->input.keys[Key::K_LEFT_SHIFT] == ButtonState::BUTTON_PRESSED)
    {
        speedMultiplier = 3.0f; // Puedes ajustar este valor para más o menos velocidad extra
    }

    float velocity = app->worldCamera.cameraSpeed * deltaTime * speedMultiplier;

    if (app->input.keys[Key::K_W] == ButtonState::BUTTON_PRESSED)
        app->worldCamera.Position += app->worldCamera.Front * velocity;
    if (app->input.keys[Key::K_S] == ButtonState::BUTTON_PRESSED)
        app->worldCamera.Position -= app->worldCamera.Front * velocity;
    if (app->input.keys[Key::K_A] == ButtonState::BUTTON_PRESSED)
        app->worldCamera.Position -= app->worldCamera.Right * velocity;
    if (app->input.keys[Key::K_D] == ButtonState::BUTTON_PRESSED)
        app->worldCamera.Position += app->worldCamera.Right * velocity;
    if (app->input.keys[Key::K_E] == ButtonState::BUTTON_PRESSED)
        app->worldCamera.Position += app->worldCamera.Up * velocity;
    if (app->input.keys[Key::K_Q] == ButtonState::BUTTON_PRESSED)
        app->worldCamera.Position -= app->worldCamera.Up * velocity;
    // Mouse-based camera rotation
    if (app->input.mouseButtons[MouseButton::RIGHT] == ButtonState::BUTTON_PRESSED)
    {
        float sensitivity = 0.1f;
        float deltaX = app->input.mouseDelta.x * sensitivity;
        float deltaY = app->input.mouseDelta.y * sensitivity;

        static float yaw = -90.0f;  // Looking toward -Z by default
        static float pitch = -17.0f; // Starting angle like in Init()

        yaw += deltaX;
        pitch -= deltaY;

        // Clamp pitch to avoid flip
        if (pitch > 89.0f)  pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;

        glm::vec3 front;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        app->worldCamera.Front = glm::normalize(front);
        app->worldCamera.Right = glm::normalize(glm::cross(app->worldCamera.Front, glm::vec3(0.0f, 1.0f, 0.0f)));
        app->worldCamera.Up = glm::normalize(glm::cross(app->worldCamera.Right, app->worldCamera.Front));
    }


    // Manejar activación/desactivación con espacio
    if (app->input.keys[Key::K_SPACE] == ButtonState::BUTTON_PRESS)
    {
        app->orbitEnabled = !app->orbitEnabled;
        if (app->orbitEnabled)
        {
            // Calcular centro de órbita (puedes ajustar esto según tu escena)
            app->orbitCenter = glm::vec3(0.0f, 5.0f, 0.0f); // Ejemplo: centro en Y=5
            app->orbitRadius = glm::distance(app->worldCamera.Position, app->orbitCenter);
        }
    }


    // Movimiento orbital si está activado
    if (app->orbitEnabled)
    {
        app->orbitAngle += app->orbitSpeed * app->deltaTime;

        // Calcular nueva posición de la cámara
        float x = app->orbitCenter.x + app->orbitRadius * cos(app->orbitAngle);
        float z = app->orbitCenter.z + app->orbitRadius * sin(app->orbitAngle);
        app->worldCamera.Position = glm::vec3(x, app->worldCamera.Position.y, z);

        // Mirar siempre al centro
        app->worldCamera.Front = glm::normalize(app->orbitCenter - app->worldCamera.Position);
        app->worldCamera.Right = glm::normalize(glm::cross(app->worldCamera.Front, glm::vec3(0.0f, 1.0f, 0.0f)));
        app->worldCamera.Up = glm::normalize(glm::cross(app->worldCamera.Right, app->worldCamera.Front));
    }
    

    // Check if camera actually moved
    if (prevPosition != app->worldCamera.Position || prevFront != app->worldCamera.Front)
    {
        // Actualizar la matriz de vista
        app->worldCamera.ViewMatrix = glm::lookAt(app->worldCamera.Position, app->worldCamera.Position + app->worldCamera.Front, app->worldCamera.Up);

        // Update all entities' VP matrices
        UpdateEntities(app);
        UpdateLights(app);
    }

    // Actualizar entidades y luces
    UpdateEntities(app);
    UpdateLights(app);

    app->time += app->deltaTime;

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

void UpdateEntitiesWithVP(App* app, const glm::mat4& VP)
{
    MapBuffer(app->entityUBO, GL_WRITE_ONLY);

    for (auto& entity : app->entities)
    {
        glm::mat4* vpOffset = (glm::mat4*)((u8*)app->entityUBO.data + entity.entityBufferOffset + sizeof(glm::mat4));
        *vpOffset = VP * entity.worldMatrix;
    }

    UnmapBuffer(app->entityUBO);
}

void RenderSceneWithClipPlane(App* app, const glm::vec4& clipPlane)
{
    Program& geometryProgram = app->programs[app->geometryProgramIdx];
    glUseProgram(geometryProgram.handle);

    // Configurar plano de recorte
    GLint clipPlaneLoc = glGetUniformLocation(geometryProgram.handle, "uClipPlane");
    if (clipPlaneLoc != -1) {
        glUniform4fv(clipPlaneLoc, 1, glm::value_ptr(clipPlane));
    }


    glEnable(GL_CLIP_DISTANCE0);

    // Renderizar todas las entidades excepto el agua
    for (const auto& entity : app->entities)
    {
        if (entity.modelIndex == app->waterModelIdx) continue;

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


    glDisable(GL_CLIP_DISTANCE0);
    glUseProgram(0);
}

void RenderIrradianceDebugCube(App* app)
{
    Program& debugProgram = app->programs[app->irradianceDebugProgramIdx]; // Asegúrate de cargarlo
    glUseProgram(debugProgram.handle);

    // Remover la traslación de la vista
    glm::mat4 view = glm::mat4(glm::mat3(app->worldCamera.ViewMatrix));
    glm::mat4 proj = app->worldCamera.ProjectionMatrix;

    glUniformMatrix4fv(glGetUniformLocation(debugProgram.handle, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(debugProgram.handle, "projection"), 1, GL_FALSE, glm::value_ptr(proj));

    // Irradiance cubemap
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, app->irradianceMap);
    glUniform1i(glGetUniformLocation(debugProgram.handle, "irradianceMap"), 0);

    glDepthFunc(GL_LEQUAL);
    glBindVertexArray(app->SkyBoxVao);  // Puedes usar tu mismo cubo de skybox
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    glDepthFunc(GL_LESS);

    glUseProgram(0);
}



void RenderWater(App* app)
{

    Program& waterProgram = app->programs[app->waterProgramIdx];
    glUseProgram(waterProgram.handle);

    // Configurar matrices
    glm::mat4 projectionMatrix = app->worldCamera.ProjectionMatrix;
    glm::mat4 worldViewMatrix = app->worldCamera.ViewMatrix;

    GLint tileSizeLoc = glGetUniformLocation(waterProgram.handle, "tileSize");
    glUniform1f(tileSizeLoc, app->waterTileSize);

    glUniformMatrix4fv(glGetUniformLocation(waterProgram.handle, "projectionMatrix"), 1, GL_FALSE, &projectionMatrix[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(waterProgram.handle, "worldViewMatrix"), 1, GL_FALSE, &worldViewMatrix[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(waterProgram.handle, "viewMatrix"), 1, GL_FALSE, &app->worldCamera.ViewMatrix[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(waterProgram.handle, "viewMatrixInv"), 1, GL_FALSE, &glm::inverse(app->worldCamera.ViewMatrix)[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(waterProgram.handle, "projectionMatrixInv"), 1, GL_FALSE, &glm::inverse(projectionMatrix)[0][0]);

    // Configurar texturas

    if (app->mode == Mode_Deferred_Geometry)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, app->reflectionFBO.attachments[0].second);
        glUniform1i(glGetUniformLocation(waterProgram.handle, "reflectionMap"), 0);


        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, app->refractionFBO.attachments[0].second);
        glUniform1i(glGetUniformLocation(waterProgram.handle, "refractionMap"), 1);
    }
    else
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, app->reflectionFBO.attachments[0].second);
        glUniform1i(glGetUniformLocation(waterProgram.handle, "reflectionMap"), 0);


        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, app->refractionFBO.attachments[0].second);
        glUniform1i(glGetUniformLocation(waterProgram.handle, "refractionMap"), 1);
    }

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, app->reflectionFBO.depthHandle);
    glUniform1i(glGetUniformLocation(waterProgram.handle, "reflectionDepth"), 2);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, app->refractionFBO.depthHandle);
    glUniform1i(glGetUniformLocation(waterProgram.handle, "refractionDepth"), 3);

    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, app->textures[app->normalMap].handle); // Normal map
    glUniform1i(glGetUniformLocation(waterProgram.handle, "normalMap"), 4);

    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, app->textures[app->dudvMap].handle); // Dudv map
    glUniform1i(glGetUniformLocation(waterProgram.handle, "dudvMap"), 5);

    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, app->textures[app->foamMap].handle);
    glUniform1i(glGetUniformLocation(waterProgram.handle, "foamMap"), 6);

    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D, app->textures[app->causticsMap].handle);
    glUniform1i(glGetUniformLocation(waterProgram.handle, "causticsMap"), 7);


    glUniform2f(glGetUniformLocation(waterProgram.handle, "viewportSize"), (float)app->displaySize.x, (float)app->displaySize.y);
    glUniform1f(glGetUniformLocation(waterProgram.handle, "time"), app->time);



    for (const auto& entity : app->entities)
    {
        if (entity.modelIndex != app->waterModelIdx) continue;
        
        glBindBufferRange(GL_UNIFORM_BUFFER, 1, app->entityUBO.handle, entity.entityBufferOffset, entity.entityBufferSize);
        

        Model& model = app->models[entity.modelIndex];
        Mesh& mesh = app->meshes[model.meshIdx];

        for (size_t i = 0; i < mesh.submeshes.size(); ++i)
        {
            GLuint vao = FindVAO(mesh, i, waterProgram);
            glBindVertexArray(vao);

            SubMesh& submesh = mesh.submeshes[i];
            glDrawElements(GL_TRIANGLES, submesh.indices.size(), GL_UNSIGNED_INT, (void*)(u64)submesh.indexOffset);
        }
    }
    glDisable(GL_BLEND);
    glUseProgram(0);
}

void RenderSkybox(App* app, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix)
{
    glDepthFunc(GL_LEQUAL);

    Program& program = app->programs[app->SkyBoxIdx];
    glUseProgram(program.handle);

    // Quitar la traslación de la matriz de vista (solo rotación para el skybox)
    glm::mat4 view = glm::mat4(glm::mat3(viewMatrix));

    glUniformMatrix4fv(glGetUniformLocation(program.handle, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(program.handle, "projection"), 1, GL_FALSE, glm::value_ptr(projectionMatrix));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, app->skyboxMap);
    glUniform1i(glGetUniformLocation(program.handle, "skybox"), 0);

    
    glBindVertexArray(app->SkyBoxVao);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);

    glDepthFunc(GL_LESS);
    glUseProgram(0);
}

void RenderSphereIBL(App* app)
{
    Program& iblProgram = app->programs[app->iblCombinedProgramIdx];
    glUseProgram(iblProgram.handle);

    // Matrices
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(0, 10, 0)); // Posición de la esfera
    glUniformMatrix4fv(glGetUniformLocation(iblProgram.handle, "uModel"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(iblProgram.handle, "uView"), 1, GL_FALSE, glm::value_ptr(app->worldCamera.ViewMatrix));
    glUniformMatrix4fv(glGetUniformLocation(iblProgram.handle, "uProjection"), 1, GL_FALSE, glm::value_ptr(app->worldCamera.ProjectionMatrix));
    glUniform3fv(glGetUniformLocation(iblProgram.handle, "uCameraPosition"), 1, glm::value_ptr(app->worldCamera.Position));

    // Bind global UBO
    glBindBufferRange(GL_UNIFORM_BUFFER, 0, app->globalUBO.handle, 0, app->globalUBO.size);

    // Texturas IBL
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, app->irradianceMap);
    glUniform1i(glGetUniformLocation(iblProgram.handle, "irradianceMap"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_CUBE_MAP, app->skyboxMap); // o prefilteredMap si usas reflexión especular
    glUniform1i(glGetUniformLocation(iblProgram.handle, "skyboxMap"), 1);

    // Uniforms opcionales
    glUniform1f(glGetUniformLocation(iblProgram.handle, "reflectionStrength"), 0.5f);
    glUniform3f(glGetUniformLocation(iblProgram.handle, "baseColor"), 1.0f, 1.0f, 1.0f);

    // Dibujar esfera
    glBindVertexArray(app->vaoSphere);
    glDrawElements(GL_TRIANGLES, app->indexCountSphere, GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);
    glUseProgram(0);
}



void RenderCube()
{
    static GLuint cubeVAO = CreateCubeVAO(); // o guarda en App*
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

void RenderGlassCube(App* app, const glm::mat4& modelMatrix)
{
    Program& refractionProgram = app->programs[app->refractionProgramIdx];
    glUseProgram(refractionProgram.handle);

    // Matrices
    glUniformMatrix4fv(glGetUniformLocation(refractionProgram.handle, "uModel"), 1, GL_FALSE, glm::value_ptr(modelMatrix));
    glUniformMatrix4fv(glGetUniformLocation(refractionProgram.handle, "uView"), 1, GL_FALSE, glm::value_ptr(app->worldCamera.ViewMatrix));
    glUniformMatrix4fv(glGetUniformLocation(refractionProgram.handle, "uProjection"), 1, GL_FALSE, glm::value_ptr(app->worldCamera.ProjectionMatrix));

    // Cámara
    glUniform3fv(glGetUniformLocation(refractionProgram.handle, "uCameraPosition"), 1, glm::value_ptr(app->worldCamera.Position));

    // Skybox
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, app->skyboxMap);
    glUniform1i(glGetUniformLocation(refractionProgram.handle, "skybox"), 0);

    // Opcional: índice de refracción
    glUniform1f(glGetUniformLocation(refractionProgram.handle, "refractRatio"), 1.0f / 1.52f);

    // Dibujar cubo (suponiendo que lo tienes cargado)
    glBindVertexArray(app->vaoRefractionCube);
    glDrawElements(GL_TRIANGLES, app->refractionCubeIndexCount, GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);
    glUseProgram(0);
}

void RenderReflectiveCube(App* app, const glm::mat4& modelMatrix)
{
    Program& reflectionProgram = app->programs[app->reflectionProgramIdx];
    glUseProgram(reflectionProgram.handle);

    glUniformMatrix4fv(glGetUniformLocation(reflectionProgram.handle, "uModel"), 1, GL_FALSE, glm::value_ptr(modelMatrix));
    glUniformMatrix4fv(glGetUniformLocation(reflectionProgram.handle, "uView"), 1, GL_FALSE, glm::value_ptr(app->worldCamera.ViewMatrix));
    glUniformMatrix4fv(glGetUniformLocation(reflectionProgram.handle, "uProjection"), 1, GL_FALSE, glm::value_ptr(app->worldCamera.ProjectionMatrix));
    glUniform3fv(glGetUniformLocation(reflectionProgram.handle, "uCameraPosition"), 1, glm::value_ptr(app->worldCamera.Position));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, app->skyboxMap);
    glUniform1i(glGetUniformLocation(reflectionProgram.handle, "skybox"), 0);

    glBindVertexArray(app->vaoRefractionCube);
    glDrawElements(GL_TRIANGLES, app->refractionCubeIndexCount, GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);
    glUseProgram(0);
}

void Render(App* app)
{

    // 1. Renderizar reflexión
    glBindFramebuffer(GL_FRAMEBUFFER, app->reflectionFBO.handle);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Configurar cámara de reflexión
    Camera reflectionCamera = app->worldCamera;
    reflectionCamera.Position.y = -reflectionCamera.Position.y; // Invertir posición Y
    reflectionCamera.Front.y = -reflectionCamera.Front.y;       // Invertir dirección Y

    // Recalcular Right y Up para la reflexión
    reflectionCamera.Right = glm::normalize(glm::cross(reflectionCamera.Front, glm::vec3(0.0f, 1.0f, 0.0f)));
    reflectionCamera.Up = glm::normalize(glm::cross(reflectionCamera.Right, reflectionCamera.Front));

    reflectionCamera.ViewMatrix = glm::lookAt(reflectionCamera.Position, reflectionCamera.Position + reflectionCamera.Front, reflectionCamera.Up);

    // Renderizar escena con plano de recorte para reflexión
    glm::mat4 reflectionVP = reflectionCamera.ProjectionMatrix * reflectionCamera.ViewMatrix;
    UpdateEntitiesWithVP(app, reflectionVP);
    

    switch (app->mode)
    {
        case Mode_Textured_Geometry: //modo textura en pantalla ( SCREEN TEXTURE SHADER )
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
            GLuint textureHandle = app->textures[app->BaseIdx].handle;
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

            RenderSceneWithClipPlane(app, glm::vec4(0, 1, 0, 0)); // Plano de recorte para reflexión
            RenderSkybox(app, reflectionCamera.ViewMatrix, reflectionCamera.ProjectionMatrix);

            // 2. Renderizar refracción
            glBindFramebuffer(GL_FRAMEBUFFER, app->refractionFBO.handle);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // Usar cámara normal para refracción
            glm::mat4 refractionVP = app->worldCamera.ProjectionMatrix * app->worldCamera.ViewMatrix;
            UpdateEntitiesWithVP(app, refractionVP);
            RenderSceneWithClipPlane(app, glm::vec4(0, -1, 0, 0)); // Plano de recorte para refracción
            //RenderIrradianceDebugCube(app);
         

            // 3. Renderizar escena principal
            glBindFramebuffer(GL_FRAMEBUFFER, app->primaryFBO.handle);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


            // Renderizar escena normal sin plano de recorte (o con uno muy lejano)
            UpdateEntitiesWithVP(app, app->worldCamera.ProjectionMatrix * app->worldCamera.ViewMatrix);

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            RenderSceneWithClipPlane(app, glm::vec4(0, -1, 0, 100.0));

            RenderSphereIBL(app);

            //glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0, 10, 0));
            //RenderGlassCube(app, modelMatrix);

            //modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 10.f, 0.0f));
            //RenderReflectiveCube(app, modelMatrix);

            // 4. Renderizar el agua
            RenderWater(app);

            RenderSkybox(app, app->worldCamera.ViewMatrix, app->worldCamera.ProjectionMatrix);
           // RenderIrradianceDebugCube(app);
           

        }
        break;

        case Mode_Deferred_Geometry:
        {
            // 1. Reflexión
            RenderSceneWithClipPlane(app, glm::vec4(0, 1, 0, 0));
            RenderSkybox(app, reflectionCamera.ViewMatrix, reflectionCamera.ProjectionMatrix);
            RenderScreenFillQuad(app, app->reflectionFBO);

            // 2. Refracción
            glBindFramebuffer(GL_FRAMEBUFFER, app->refractionFBO.handle);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glm::mat4 refractionVP = app->worldCamera.ProjectionMatrix * app->worldCamera.ViewMatrix;
            UpdateEntitiesWithVP(app, refractionVP);
            RenderSceneWithClipPlane(app, glm::vec4(0, -1, 0, 0));
            RenderSkybox(app, app->worldCamera.ViewMatrix, app->worldCamera.ProjectionMatrix);
            RenderScreenFillQuad(app, app->refractionFBO);

            // 3. Escena principal
            glBindFramebuffer(GL_FRAMEBUFFER, app->primaryFBO.handle);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            std::vector<GLuint> textures;
            for (auto& it : app->primaryFBO.attachments)
                textures.push_back(it.second);
            glDrawBuffers(textures.size(), textures.data());

            //RenderIrradianceDebugCube(app);
            //RenderSkybox(app, app->worldCamera.ViewMatrix, app->worldCamera.ProjectionMatrix);

            // Renderizar escena
            UpdateEntitiesWithVP(app, app->worldCamera.ProjectionMatrix * app->worldCamera.ViewMatrix);
            RenderSceneWithClipPlane(app, glm::vec4(0, -1, 0, 100.0));
            RenderWater(app);
            RenderSphereIBL(app);

            //glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0, 10, 0));
            //RenderGlassCube(app, modelMatrix);

            //modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 10.f, 0.0f));
            //RenderReflectiveCube(app, modelMatrix);

            // 4. Final: compositar quad con G-Buffers
           glBindFramebuffer(GL_FRAMEBUFFER, 0);
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



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


    GLint displayModeLoc = glGetUniformLocation(programTexturedGeometry.handle, "uDisplayMode");
    glUniform1i(displayModeLoc, static_cast<int>(app->deferredDisplayMode));

    GLint invertDepthLoc = glGetUniformLocation(programTexturedGeometry.handle, "uInvertDepth");
    glUniform1i(invertDepthLoc, static_cast<int>(app->InverseDepth));


    size_t iteration = 0;

    const char* uniformNames[] = { "uAlbedo", "uNormals", "uPosition", "uViewDir" };
    for (const auto& texture : aFBO.attachments)
    {
        GLint uniformPosition = glGetUniformLocation(programTexturedGeometry.handle, uniformNames[iteration]);

        glActiveTexture(GL_TEXTURE0 + iteration);
        glBindTexture(GL_TEXTURE_2D, texture.second);
        glUniform1i(uniformPosition, iteration);
        ++iteration;
    }
    
    GLint uniformPosition = glGetUniformLocation(programTexturedGeometry.handle, "uDepth");
    glActiveTexture(GL_TEXTURE0 + iteration);
    glBindTexture(GL_TEXTURE_2D, aFBO.depthHandle);
    glUniform1i(uniformPosition, iteration);
    

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

    //app->ModelIdx = LoadModel(app, "Patrick/Patrick.obj");
    app->ModelIdx = LoadModel(app, "Queen/Queen.obj");
    u32 planeIdx = LoadModel(app, "Plane/Plane.obj");

    app->geometryProgramIdx = LoadProgram(app, "RENDER_GEOMETRY.glsl", "RENDER_GEOMETRY_DEFERRED");
    app->ModelTextureUniform = glGetUniformLocation(app->programs[app->geometryProgramIdx].handle, "uTexture");

    //Class06
  
    float aspectRatio = (float)app->displaySize.x / (float)app->displaySize.y;
    
    app->worldCamera.ProjectionMatrix = glm::perspective(glm::radians(60.f), aspectRatio, app->worldCamera.nearPlane, app->worldCamera.farPlane);
    app->worldCamera.Position = vec3(0, 7, 6);
    app->worldCamera.Front = glm::vec3(0.0f, -0.3f, -1.0f); // Apuntando ligeramente hacia abajo
    app->worldCamera.ViewMatrix = glm::lookAt(app->worldCamera.Position, app->worldCamera.Position + app->worldCamera.Front, app->worldCamera.Up);
    
    glm::mat4 VP = app->worldCamera.ProjectionMatrix * app->worldCamera.ViewMatrix;
    
    glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &app->maxUniformBufferSize);
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &app->uniformBlockAligment);

    app->globalUBO = CreateConstantBuffer(app->maxUniformBufferSize);
    app->entityUBO = CreateConstantBuffer(app->maxUniformBufferSize);

    //Lights
    app->lights.push_back({ LightType::Light_Directional, vec3(0.4), vec3(-1,-1,-1),vec3(0)});
    app->lights.push_back({ LightType::Light_Directional, vec3(0.2,0,0), vec3(0.5,0,1),vec3(0)});
    app->lights.push_back({ LightType::Light_Point, vec3(1,0.5,0.5), vec3(0),vec3({0, 8.5, 1}) });
    app->lights.push_back({ LightType::Light_Point, vec3(0.5,0.5,1), vec3(0),vec3(-5, 8.5, -3) });
    app->lights.push_back({ LightType::Light_Point, vec3(0.5,1,0.5), vec3(0),vec3(5, 8.5, -3) });
    app->lights.push_back({ LightType::Light_Point, vec3(0.5,0.5,0), vec3(0),vec3(0, 8.5, -7) });

    UpdateLights(app);

    //Entities Buffer
    Buffer& entityUBO = app->entityUBO;
    MapBuffer(entityUBO, GL_WRITE_ONLY);

    std::vector<glm::vec3> positions = {
     {0, 0, 0},   // Entidad central al frente
     {-5, 0, -3}, // Entidad izquierda atrás
     {5, 0, -3},  // Entidad derecha atrás
     {0, 0, -7},   // Entidad central al frente
    };

    //Geometry Plane
    CreateEntity(app, planeIdx, VP, positions[0]);

    //Geometry Queens
    for (const auto& pos : positions)
    {
      CreateEntity(app, app->ModelIdx, VP, pos);
    }

    UnmapBuffer(entityUBO);

    app->mode = Mode_Deferred_Geometry;
    app->primaryFBO.CreateFBO(4, app->displaySize.x, app->displaySize.y);

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

    // Modo Rendering
    ImGui::Text(" Render Mode");
    if (ImGui::Button(app->mode == Mode_Forward_Geometry ? " Switch to Deferred" : " Switch to Forward", ImVec2(220, 35)))
    {
        app->mode = (app->mode == Mode_Forward_Geometry) ? Mode_Deferred_Geometry : Mode_Forward_Geometry;

        app->texturedGeometryProgramIdx = LoadProgram(app, "RENDER_QUAD.glsl", app->mode == Mode_Deferred_Geometry ? "RENDER_QUAD_DEFERRED" : "RENDER_QUAD_FORWARD");
        app->geometryProgramIdx = LoadProgram(app, "RENDER_GEOMETRY.glsl", app->mode == Mode_Deferred_Geometry ? "RENDER_GEOMETRY_DEFERRED" : "RENDER_GEOMETRY_FORWARD");

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

    ImGui::End();

    ImGui::PopStyleColor(8); // Restore all colors
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
    float speedMultiplier = 1.0f;
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

    // Check if camera actually moved
    if (prevPosition != app->worldCamera.Position || prevFront != app->worldCamera.Front)
    {
        // Actualizar la matriz de vista
        app->worldCamera.ViewMatrix = glm::lookAt(app->worldCamera.Position,
            app->worldCamera.Position + app->worldCamera.Front,
            app->worldCamera.Up);

        // Update all entities' VP matrices
        UpdateEntities(app);
    }
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
            GLuint textureHandle = app->textures[app->ModelIdx].handle;
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
            glClearColor(0.0, 0.0, 0.0, 0.0);

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



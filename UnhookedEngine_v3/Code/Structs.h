#ifndef STRUCTS
#define STRUCTS

#include"platform.h"
#include "EnvironmentMapping.h"
#include <glad/glad.h> 
#include <stdexcept>    


typedef glm::vec2  vec2;
typedef glm::vec3  vec3;
typedef glm::vec4  vec4;
typedef glm::ivec2 ivec2;
typedef glm::ivec3 ivec3;
typedef glm::ivec4 ivec4;

class Environment;

struct Camera {
    glm::mat4 ViewMatrix;
    glm::mat4 ProjectionMatrix;
    glm::vec3 Position;
    glm::vec3 Front = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 Up = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 Right = glm::vec3(1.0f, 0.0f, 0.0f);

    // Control de c�mara
    float cameraSpeed = 5.0f;

    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
};

struct Buffer {
    u32 size;
    GLenum type;
    GLuint handle;
    u32 head;
    u8* data;
};

struct Image
{
    void* pixels;
    ivec2 size;
    i32   nchannels;
    i32   stride;
};

struct Texture
{
    GLuint      handle;
    std::string filepath;
};

struct VertexShaderAttribute {
    u8 location;
    u8 componentCount;
};

struct VertexShaderLayout {
    std::vector<VertexShaderAttribute> attributes;
};

struct Program
{
    GLuint handle;
    std::string filepath;
    std::string programName;
    u64 lastWriteTimestamp; 
    VertexShaderLayout vertexInputLayout;
};

enum Mode
{
    Mode_Textured_Geometry,
    Mode_Deferred_Geometry,
    Mode_Forward_Geometry,
    Mode_Count
};

struct VertexV3V2
{
    glm::vec3 pos;
    glm::vec2 uv;
};

const VertexV3V2 vertices[] = {

    {   glm::vec3(-1,-1,0.0), glm::vec2(0.0,0.0)    },  //Bottom-left verex
    {   glm::vec3(1,-1,0.0),  glm::vec2(1.0,0.0)    },  //Bottom-Right vertex
    {   glm::vec3(1,1,0.0),   glm::vec2(1.0,1.0)    },  //Top_Right vertex
    {   glm::vec3(-1,1,0.0),  glm::vec2(0.0,1.0)    },  //Top_Left vertex
};

const u16 indices[] = {

    0, 1, 2,
    0, 2, 3
};


struct VertexBufferAttribute {
    u8 location;
    u8 componentCount;
    u8 offset;
};

struct VertexBufferLayout {
    std::vector<VertexBufferAttribute> attributes;
    u8 stride;
};

struct Model {
    u32 meshIdx;
    std::vector<u32> materialIdx;
};

struct Vao
{
    GLuint handle;
    GLuint programHandle;
};

struct SubMesh {
    VertexBufferLayout vertexBufferLayout;
    std::vector<float> vertices;
    std::vector<u32> indices;
    u32 vertexOffset;
    u32 indexOffset;
    std::vector<Vao> vaos;
};

struct Mesh {
    std::vector<SubMesh> submeshes;
    GLuint vertexBufferHandle;
    GLuint indexBufferHandle;
};

struct Material {
    std::string name;
    vec3 albedo;
    vec3 emissive;
    f32 smoothness;
    u32 albedoTextureIdx;
    u32 emissiveTextureIdx;
    u32 specularTextureIdx;
    u32 normalsTextureIdx;
    u32 bumpTextureIdx;
};

struct Entity {

    glm::mat4 worldMatrix;
    u32 modelIndex;
    u32 entityBufferSize;
    u32 entityBufferOffset;

};

enum LightType
{
    Light_Directional,
    Light_Point,

};
struct Light
{
    LightType type;
    vec3 color;
    vec3 direction;
    vec3 position;
};

struct FrameBuffer
{
    GLuint handle;
    std::vector<std::pair<GLenum, GLuint>> attachments;
    GLuint depthHandle;
    
    bool CreateFBO( const uint64_t aAttachments, const uint64_t aWidth, const uint64_t aHeight)
    {
        if (aAttachments > GL_MAX_COLOR_ATTACHMENTS)
        {
            return false;
        }

        //Color
        std::vector<GLenum> enums;
        for (size_t i = 0; i < aAttachments; ++i)
        {
            GLuint colorAttachment;
            glGenTextures(1, &colorAttachment);
            glBindTexture(GL_TEXTURE_2D, colorAttachment);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, aWidth, aHeight, 0, GL_RGBA, GL_FLOAT, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glBindTexture(GL_TEXTURE_2D, 0);
            attachments.push_back({ GL_COLOR_ATTACHMENT0 + i, colorAttachment });
            enums.push_back(GL_COLOR_ATTACHMENT0 + i);
        }

        //Depth 
        glGenTextures(1, &depthHandle);
        glBindTexture(GL_TEXTURE_2D, depthHandle);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, aWidth, aHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);


        glGenFramebuffers(1, &handle);
        glBindFramebuffer(GL_FRAMEBUFFER, handle);
        for (auto it = attachments.cbegin(); it != attachments.cend(); ++it)
        {
            glFramebufferTexture(GL_FRAMEBUFFER, it->first, it->second, 0);
        }

        glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthHandle, 0);

        //Check status

        GLenum framebufferStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (framebufferStatus != GL_FRAMEBUFFER_COMPLETE)
        {
            throw std::runtime_error("Framebuffer creation error");
        }

        glDrawBuffers(enums.size(), enums.data());
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return true;
    }


    void Clean()
    {
        glDeleteFramebuffers(1, &handle);
        for (auto& texture : attachments)
        {
            glDeleteTextures(1, &texture.second);
            texture.second = 0;
        }
        glDeleteTextures(1, &depthHandle);
        depthHandle = 0;
    }
};

enum class DeferredDisplayMode {
    Default,
    Albedo,
    Normals,
    Position,
    ViewDir,
    Depth
};

enum WaterScenePart {
    Reflection,
    Refraction
};

enum SceneType {
    Scene_Rocks,
    Scene_Island
};

struct App
{
    // Loop
    f32  deltaTime;
    bool isRunning;

    // Input
    Input input;

    // Graphics
    char gpuName[64];
    char openGlVersion[64];

    std::string mOpenGLInfo;

    ivec2 displaySize;

    std::vector<Texture>  textures;
    std::vector<Material>  materials;
    std::vector<Mesh>  meshes;
    std::vector<Model>  models;
    std::vector<Program>  programs;

    GLuint cubeVAO = 0;
    GLuint cubeVBO = 0;


    // program indices
    u32 texturedGeometryProgramIdx;
    u32 geometryProgramIdx;
    u32 waterProgramIdx;
    u32 refractionProgramIdx;
    u32 reflectionProgramIdx;
    u32 irradianceConvolutionProgramIdx;
    u32 prefilterConvolutionProgramIdx;
    u32 iblCombinedProgramIdx;
    u32 equirectToCubemapProgramIdx;

    //Water Textures
    u32 dudvMap;
    u32 normalMap;
    u32 foamMap;
    u32 causticsMap;
    u32 skyboxMap;
    u32 irradianceMap;
    u32 prefilteredMap;

    //Modelo 3D cargado
    u32 BaseIdx;
    u32 SculptIdx;
   

    u32 GrassIdx;
    u32 SandIdx;
    u32 RocksIdx;
    u32 ReddoIdx;
    u32 MetalIdx;
    u32 WoodIdx;
    u32 WindowsIdx;
    u32 StoneIdx;
    u32 SkyBoxIdx;
    u32 irradianceDebugProgramIdx;


    u32 waterModelIdx;
    u32 sphereModelIdx;

    u32 ModelTextureUniform;
    u32 planeIdx;

    GLuint vaoSphere = 0;
    GLuint vboSphere = 0;
    GLuint iboSphere = 0;
    u32 indexCountSphere = 0;

    GLuint vaoRefractionCube = 0;
    GLuint vboRefractionCube = 0;
    GLuint iboRefractionCube = 0;
    u32 refractionCubeIndexCount = 0;

    // Mode
    Mode mode;

    // Embedded geometry (in-editor simple meshes such as
    // a screen filling quad, a cube, a sphere...)
    Buffer quadVertexBuffer;
    GLuint embeddedElements;

    // Location of the texture uniform in the textured quad shader
    GLuint programUniformTexture;

    // VAO object to link our screen filling quad with our textured quad shader
    GLuint vao;
    GLuint SkyBoxVao;

    Camera worldCamera;
    GLint maxUniformBufferSize;
    GLint uniformBlockAligment;

    Buffer entityUBO;
    Buffer globalUBO;

    std::vector<Entity> entities;
    std::vector<Light> lights;

    FrameBuffer primaryFBO;
    DeferredDisplayMode deferredDisplayMode = DeferredDisplayMode::Default;
    bool InverseDepth;

    FrameBuffer reflectionFBO;
    FrameBuffer refractionFBO;

    float waterTileSize = 10.0f;
    // Uniform location para el plano de recorte (usado en reflection/refraction)
    GLint clipPlaneUniformLoc;

    // Tiempo para animaciones
    float time = 0.0f;

    SceneType currentScene = Scene_Rocks; // Escena inicial

    bool orbitEnabled = false;
    float orbitRadius = 15.0f;  // Distancia desde el punto de inter�s
    float orbitSpeed = 0.2f;    // Velocidad de rotaci�n
    float orbitAngle = 0.0f;    // �ngulo actual
    glm::vec3 orbitCenter = glm::vec3(0.0f); // Punto alrededor del cual orbitar


    Environment* environnmentComponent = nullptr;

};


#endif // !STRUCTS

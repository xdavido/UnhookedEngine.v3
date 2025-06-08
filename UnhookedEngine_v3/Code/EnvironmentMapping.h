#pragma once
#include <glad/glad.h>
#include <sstream>
#include <iostream>


#include "Structs.h"

class App;


class Environment
{
public:
    Environment();
    ~Environment();

    void LoadHDRI(App* app, const std::string& filepath);

    void GenerateIrradiance(App* app);
                      // Convolución difusa


    void SetToneMapping(bool enabled, float exposure = 1.0f);
    void SetReflectionMode(bool useDiffuse, bool useSpecular);

    void BindMaps(GLuint shaderProgram);          // Bindea cubemaps como samplers

    GLuint GetCubemap() const { return cubemap; }
    GLuint GetIrradianceMap() const { return irradianceMap; }
    GLuint GetPrefilteredMap() const { return prefilteredMap; }

    float toneExposure = 1.0f;

private:
    GLuint cubemap = 0;
    GLuint irradianceMap = 0;
    GLuint prefilteredMap = 0;

    bool toneMappingEnabled = false;
    

    bool useDiffuse = true;
    bool useSpecular = true;

    GLuint cubeVAO = 0;

};

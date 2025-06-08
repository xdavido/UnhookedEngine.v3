#include "EnvironmentMapping.h"
#include"engine.h"
#include <stb_image.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

Environment::Environment() {
    
}

Environment::~Environment() {
    glDeleteTextures(1, &cubemap);
    glDeleteTextures(1, &irradianceMap);
    glDeleteTextures(1, &prefilteredMap);
    glDeleteVertexArrays(1, &cubeVAO);
}

void Environment::LoadHDRI(App* app,const std::string& filepath) {

    if (app->skyboxMap != 0) {
        glDeleteTextures(1, &app->skyboxMap);
    }
    app->skyboxMap = ConvertHDRIToCubemap(app,filepath.c_str());
}

void Environment::GenerateIrradiance(App* app) {
    CreateIrradianceMap(app);
}


void Environment::SetToneMapping(bool enabled, float exposure) {
    toneMappingEnabled = enabled;
    toneExposure = exposure;
}

void Environment::SetReflectionMode(bool useDiffuseIBL, bool useSpecularIBL) {
    useDiffuse = useDiffuseIBL;
    useSpecular = useSpecularIBL;
}

void Environment::BindMaps(GLuint shaderProgram) {
    GLint loc;

    if (useDiffuse && irradianceMap) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);
        loc = glGetUniformLocation(shaderProgram, "irradianceMap");
        if (loc >= 0) glUniform1i(loc, 0);
    }

    if (useSpecular && prefilteredMap) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_CUBE_MAP, prefilteredMap);
        loc = glGetUniformLocation(shaderProgram, "prefilteredMap");
        if (loc >= 0) glUniform1i(loc, 1);
    }

    loc = glGetUniformLocation(shaderProgram, "toneMappingEnabled");
    if (loc >= 0) glUniform1i(loc, toneMappingEnabled);

    loc = glGetUniformLocation(shaderProgram, "toneExposure");
    if (loc >= 0) glUniform1f(loc, toneExposure);
}



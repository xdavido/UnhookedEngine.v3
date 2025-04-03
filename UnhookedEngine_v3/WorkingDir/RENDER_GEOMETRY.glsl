#ifdef RENDER_GEOMETRY
#if defined(VERTEX) ////////////////////////////////////////

layout(location=0) in vec3 aPosition;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aTexCoord;
layout(location=3) in vec3 aTangent;
layout(location=4) in vec3 aBitangent;

struct Light
{
    unsigned int type;
    vec3 color;
    vec3 direction;
    vec3 position;
};

layout(binding = 0, std140) uniform GlobalParams
{
    vec3 uCameraPosition;
    unsigned int uLightCount;
    Light uLight[16];
};

layout(binding = 1, std140) uniform EntityParams
{
  mat4 uWorldMatrix;
  mat4 uWorldViewProjectionMatrix;

};

out vec2 vTexCoord;
out vec3 vPosition;
out vec3 vNormal;
out vec3 vViewDir;


void main()
{
    vTexCoord = aTexCoord;
    vPosition = vec3(uWorldMatrix * vec4(aPosition,1.0));
    vNormal = vec3(uWorldMatrix * vec4(aNormal, 0.0));
    gl_Position = uWorldViewProjectionMatrix * vec4(aPosition,1.0);
   
}

#elif defined(FRAGMENT) ////////////////////////////////////////


struct Light
{
    unsigned int type;
    vec3 color;
    vec3 direction;
    vec3 position;
};

layout(binding = 0, std140) uniform GlobalParams
{
    vec3 uCameraPosition;
    unsigned int uLightCount;
    Light uLight[16];
};

in vec2 vTexCoord;
in vec3 vPosition;
in vec3 vNormal;
in vec3 vViewDir;

uniform sampler2D uTexture;
layout(location=0) out vec4 oColor;


vec3 CalcDirLight(Light alight, vec3 aNormal, vec3 aViewDir)
{
    vec3 lightDir = normalize(-alight.direction);
    float diff = max(dot(aNormal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, aNormal);
    float spec = pow(max(dot(aViewDir, reflectDir), 0.0), 2.0);

    vec3 ambient = alight.color * 0.2;
    vec3 diffuse = alight.color * diff;
    vec3 specular = 0.1 * spec * alight.color;
    return (ambient + diffuse + specular);
}

vec3 CalcPointLight(Light alight, vec3 aNormal, vec3 aPosition, vec3 aViewDir) {
    vec3 lightDir = normalize(alight.position - aPosition);
    float diff = max(dot(aNormal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, aNormal);
    float spec = pow(max(dot(aViewDir, reflectDir), 0.0), 32.0);  // Increased shininess

    float distance = length(alight.position - aPosition);
    
    float constant = 1.0;
    float linear = 0.07;     
    float quadratic = 0.017;  

    float radius = 7.0; 
    float attenuation = smoothstep(radius, radius * 0.2, distance);

    vec3 ambient = alight.color * 0.2;
    vec3 diffuse = alight.color * diff;
    vec3 specular = vec3(0.2) * spec * alight.color;  // Increased specular
    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;
    
    return (ambient + diffuse + specular);
}

void main()
{
    vec3 returnColor = vec3(0.0);

    for(int i = 0 ; i < uLightCount; ++i)
    {
       if(uLight[i].type == 0)
       {
        returnColor += CalcDirLight(uLight[i], vNormal, vViewDir);
       }
       else if(uLight[i].type == 1)
       {
        returnColor += CalcPointLight(uLight[i], vNormal, vPosition, vViewDir);       
       }
    }

    oColor = vec4(returnColor, 1.0);
}

#endif
#endif


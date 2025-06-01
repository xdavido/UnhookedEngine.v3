#ifdef RENDER_GEOMETRY_FORWARD
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


uniform vec4 uClipPlane;  // Nuevo uniform para el plano de recorte

out float gl_ClipDistance[1];  // Salida para el clipping
out vec2 vTexCoord;
out vec3 vPosition;
out vec3 vNormal;
out vec3 vViewDir;


void main()
{
    vTexCoord = aTexCoord;
    vPosition = vec3(uWorldMatrix * vec4(aPosition,1.0));
    vNormal = vec3(uWorldMatrix * vec4(aNormal, 0.0));
    vViewDir = uCameraPosition - vPosition;

    vec4 worldPos = uWorldMatrix * vec4(aPosition, 1.0);
   vec4 clipDistance = vec4 (0.0,0.0,0.0, length(vPosition) / 100.0);
   gl_ClipDistance[0] = dot(worldPos, uClipPlane + clipDistance);

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


vec3 CalcDirLight(Light alight, vec3 normal, vec3 viewDir, vec3 albedo)
{
    vec3 lightDir = normalize(-alight.direction);
    float diff = max(dot(normal, lightDir), 0.0);
    float angleFactor = pow(diff, 0.7);

    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 16.0);

    // Reduce la luz ambiental y hazla dependiente del ángulo para suavizar
    vec3 ambient = alight.color * 0.05 + (0.15 * diff) * alight.color;
    vec3 diffuse = angleFactor * alight.color * albedo * 0.8;
    vec3 specular = spec * alight.color * 0.4;

    return ambient + diffuse + specular;
}

vec3 CalcPointLight(Light pointLight, vec3 normal, vec3 position, vec3 viewDir, vec3 albedo)
{
    vec3 lightDir = normalize(pointLight.position - position);
    float distance = length(pointLight.position - position);

    float radius = 20.0; // Rango más grande
    float attenuation = clamp(1.0 - distance / radius, 0.0, 1.0);
    attenuation *= attenuation; // curva suave

    float diff = max(dot(normal, lightDir), 0.0);
    float angleFactor = pow(diff, 0.8); // más abierto
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);

    vec3 ambient = pointLight.color * 0.15;
    vec3 diffuse = angleFactor * pointLight.color * albedo * 0.7;
    vec3 specular = spec * pointLight.color * 0.5;

    return attenuation * (ambient + diffuse + specular);
}

void main()
{
    vec3 albedo = texture(uTexture, vTexCoord).rgb;
    vec3 normal = normalize(vNormal);
    vec3 viewDir = normalize(vViewDir);
    vec3 returnColor = vec3(0.0);

    for(int i = 0; i < uLightCount; ++i)
    {
       if(uLight[i].type == 0) // Directional light
       {
            returnColor += CalcDirLight(uLight[i], normal, viewDir, albedo);
       }
       else if(uLight[i].type == 1) // Point light
       {
            returnColor += CalcPointLight(uLight[i], normal, vPosition, viewDir, albedo);       
       }
    }

    oColor = vec4(returnColor, 1.0);
}

#endif
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef RENDER_GEOMETRY_DEFERRED
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


uniform vec4 uClipPlane;  // Nuevo uniform para el plano de recorte

out float gl_ClipDistance[1];  // Salida para el clipping
out vec2 vTexCoord;
out vec3 vPosition;
out vec3 vNormals;
out vec3 vViewDir;


void main()
{
    vTexCoord = aTexCoord;
    vPosition = vec3(uWorldMatrix * vec4(aPosition,1.0));
     mat3 normalMatrix = transpose(inverse(mat3(uWorldMatrix)));
    vNormals = normalize(normalMatrix * aNormal);

    vViewDir = normalize(uCameraPosition - vPosition);

    vec4 worldPos = uWorldMatrix * vec4(aPosition, 1.0);
    gl_ClipDistance[0] = dot(worldPos, uClipPlane);

    gl_Position = uWorldViewProjectionMatrix * vec4(aPosition,1.0);
   
}

#elif defined(FRAGMENT) ////////////////////////////////////////

in vec2 vTexCoord;
in vec3 vPosition;
in vec3 vNormals;
in vec3 vViewDir;

uniform sampler2D uTexture;

layout(location=0) out vec4 oAlbedo;
layout(location=1) out vec4 oNormals;
layout(location=2) out vec4 oPosition;
layout(location=3) out vec4 oViewDir;


void main()
{
    oAlbedo = texture(uTexture, vTexCoord);
    oNormals = vec4(vNormals, 1.0); 
    oPosition = vec4(vPosition, 1.0);
    oViewDir = vec4(vViewDir, 1.0);


}


#endif
#endif
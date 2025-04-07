#ifdef RENDER_QUAD

#if defined(VERTEX) ////////////////////////////////////////

layout(location=0) in vec3 aPosition;
layout(location=1) in vec2 aTexCoord;


out vec2 vTexCoord;

void main()
{
    vTexCoord = aTexCoord;
    gl_Position = vec4(aPosition, 1.0);
   
}

#elif defined(FRAGMENT) ////////////////////////////////////////

in vec2 vTexCoord;

uniform sampler2D uTexture;

layout(location=0) out vec4 oColor;

void main()
{
    oColor = texture(uTexture, vTexCoord);
}

#endif
#endif
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef RENDER_QUAD_DEFERRED

#if defined(VERTEX) ////////////////////////////////////////

layout(location=0) in vec3 aPosition;
layout(location=1) in vec2 aTexCoord;


out vec2 vTexCoord;

void main()
{
    vTexCoord = aTexCoord;
    gl_Position = vec4(aPosition, 1.0);
   
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

uniform sampler2D uAlbedo;
uniform sampler2D uNormals;
uniform sampler2D uPosition;
uniform sampler2D uViewDir;

layout(location=0) out vec4 oColor;


// Direccional con atenuación angular más suave
vec3 CalcDirLight(Light alight, vec3 normal, vec3 viewDir, vec3 albedo)
{
    vec3 lightDir = normalize(-alight.direction);
    float diff = max(dot(normal, lightDir), 0.0);
    float angleFactor = pow(diff, 0.7); // dispersión más abierta

    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 16.0);

    vec3 ambient = alight.color * 0.2;
    vec3 diffuse = angleFactor * alight.color * albedo * 0.8;
    vec3 specular = spec * alight.color * 0.4;

    return ambient + diffuse + specular;
}


// Punto con atenuación suavizada
vec3 CalcPointLight(Light pointLight, vec3 normal, vec3 position, vec3 viewDir, vec3 albedo)
{
    vec3 lightDir = normalize(pointLight.position - position);
    float distance = length(pointLight.position - position);

    float radius = 5.0; // Rango más grande
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
    vec3 Albedo = texture(uAlbedo, vTexCoord).rgb;
    vec3 Normal = normalize(texture(uNormals, vTexCoord).xyz);
    vec3 Position = texture(uPosition, vTexCoord).xyz;
    vec3 ViewDir = normalize(texture(uViewDir, vTexCoord).xyz);

    vec3 returnColor = vec3(0.0);

    for(int i = 0 ; i < uLightCount; ++i)
    {
        if(uLight[i].type == 0)
        {
            returnColor += CalcDirLight(uLight[i], Normal, ViewDir, Albedo);
        }
        else if(uLight[i].type == 1)
        {
            returnColor += CalcPointLight(uLight[i], Normal, Position, ViewDir, Albedo);       
        }
    }

    oColor = vec4(returnColor, 1.0);
}


#endif
#endif


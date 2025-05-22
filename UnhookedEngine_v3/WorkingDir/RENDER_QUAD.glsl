#ifdef RENDER_QUAD_FORWARD

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
uniform sampler2D uDepth;

uniform float near = 0.1;
uniform float far = 100.0;
    
uniform int uDisplayMode;
uniform bool uInvertDepth;

layout(location=0) out vec4 oColor;


vec3 CalcDirLight(Light alight, vec3 normal, vec3 position, vec3 viewDir, vec3 albedo)
{
    vec3 lightDir = normalize(-alight.direction);
    float diff = max(dot(normal, lightDir), 0.0);
    float angleFactor = pow(diff, 0.7);

    vec3 reflectDir = normalize(reflect(-lightDir, normal));
vec3 fakeViewDir = normalize(uCameraPosition - position);

    float spec = pow(max(dot(fakeViewDir, reflectDir), 0.0), 16.0);

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

    float depth = texture(uDepth, vTexCoord).r;
    if (depth >= 1.0 - 1e-5) // Evita precision issues
    {
        oColor = vec4(0.0, 0.0, 0.0, 1.0); // Fondo negro
        return;
    }

    vec3 Albedo = texture(uAlbedo, vTexCoord).rgb;
    vec3 Normal = normalize(texture(uNormals, vTexCoord).xyz);
    vec3 Position = texture(uPosition, vTexCoord).xyz;
    vec3 ViewDir = normalize(texture(uViewDir, vTexCoord).xyz);
    
   
    switch (uDisplayMode)
    {
        case 0:  // Default: Iluminación con luces
        
           vec3 returnColor = vec3(0.0);
    for (int i = 0; i < uLightCount; ++i)
    {
        if (uLight[i].type == 0) // Directional light
            returnColor += CalcDirLight(uLight[i], Normal,Position, ViewDir, Albedo);
        else if (uLight[i].type == 1) // Point light
            returnColor += CalcPointLight(uLight[i], Normal, Position, ViewDir, Albedo);
    }

    oColor = vec4(returnColor, 1.0);
            break;

        case 1:  // Albedo
            oColor = vec4(Albedo, 1.0);
            break;

        case 2:  // Normals
           oColor = vec4(Normal, 1.0); // Visualiza las normales

            break;

        case 3:  // Position
            oColor = vec4(Position, 1.0);
            break;

        case 4:  // ViewDir
            oColor = vec4(ViewDir, 1.0);
            break;

        case 5:  // Linearized depth

            float depthRaw = texture(uDepth, vTexCoord).r;
            float z = depthRaw * 2.0 - 1.0; // Convertir a NDC
            float linearDepth = (2.0 * near * far) / (far + near - z * (far - near));
            float normalized = clamp(linearDepth / far, 0.0, 1.0);
         
            oColor = vec4(vec3(normalized), 1.0);
            if (uInvertDepth)
            {
                float inverseDepth = 1.0 - normalized;
                oColor = vec4(vec3(inverseDepth), 1.0);
            }

            break;

      default:
            oColor = vec4(vec3(1.0, 0.0, 1.0), 1.0); // Magenta si algo falla
            break;
    }

}

#endif
#endif

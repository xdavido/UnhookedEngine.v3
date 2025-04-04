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
    vec3 specular = 0.1 * spec * alight.color; 
    
    return (ambient + diffuse + specular);
}

void main()
{
    vec3 Albedo = vec3(texture(uAlbedo, vTexCoord));

    vec3 Normal = texture(uNormals, vTexCoord).xyz;
    vec3 Position = texture(uPosition, vTexCoord).xyz;
    vec3 ViewDir = texture(uViewDir, vTexCoord).xyz;

    vec3 returnColor = vec3(0.0);

    for(int i = 0 ; i < uLightCount; ++i)
    {
        vec3 lightResult = vec3(0);
       if(uLight[i].type == 0)
       {
            lightResult += CalcDirLight(uLight[i],Normal, ViewDir);
       }
       else if(uLight[i].type == 1)
       {
            lightResult += CalcPointLight(uLight[i], Normal, Position, ViewDir);       
       }
       returnColor.rgb += lightResult * Albedo;
    }

    oColor = vec4(returnColor, 1.0);
}

#endif
#endif


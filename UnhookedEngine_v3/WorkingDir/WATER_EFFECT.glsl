#ifdef WATER_EFFECT

#if defined(VERTEX) ////////////////////////////////////////
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;

uniform mat4 projectionMatrix;
uniform mat4 worldViewMatrix;

out Data
{
    vec3 positionViewspace;
    vec3 normalViewspace;
} VSOut;

void main()
{
    VSOut.positionViewspace = vec3(worldViewMatrix * vec4(position, 1.0));
    VSOut.normalViewspace = mat3(worldViewMatrix) * normal;
    gl_Position = projectionMatrix * vec4(VSOut.positionViewspace, 1.0);
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

uniform vec2 viewportSize;
uniform float time;
uniform float tileSize; 

uniform mat4 viewMatrix;
uniform mat4 viewMatrixInv;
uniform mat4 projectionMatrixInv;

uniform sampler2D reflectionMap;
uniform sampler2D refractionMap;
uniform sampler2D reflectionDepth;
uniform sampler2D refractionDepth;
uniform sampler2D normalMap;
uniform sampler2D dudvMap;
uniform sampler2D foamMap;

// Parámetros ajustables para el efecto de agua
const float FOAM_FALLOFF_DISTANCE = 1.0;
const float FOAM_FALLOFF_BIAS = 1.5;
const float FOAM_LEADING_EDGE = 0.75;
const float FOAM_SCROLL_SPEED = 0.15;
const float FOAM_TILING = 3.0;
const float FOAM_SCALE = 0.15;

// Nuevos uniformes para control del color del agua
uniform vec3 waterColor = vec3(0.25, 0.4, 0.6);
uniform float turbidityDistance = 2.0;  // Distancia para transición completa a color profundo

in Data {
    vec3 positionViewspace;
    vec3 normalViewspace;
} FSIn;

out vec4 outColor;

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 reconstructPixelPosition(float depth) {
    vec2 texCoords = gl_FragCoord.xy / viewportSize;
    vec3 positionNDC = vec3(texCoords * 2.0 - vec2(1.0), depth * 2.0 - 1.0);
    vec4 positionEyespace = projectionMatrixInv * vec4(positionNDC, 1.0);
    positionEyespace.xyz /= positionEyespace.w;
    return positionEyespace.xyz;
}

vec3 CalcDirLight(Light alight, vec3 normal, vec3 viewDir)
{
    vec3 lightDir = normalize(mat3(viewMatrix) * (-alight.direction));
    
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 128.0);
    vec3 specular = spec * alight.color * 0.6;
    
    return specular;
}

vec3 CalcPointLight(Light pointLight, vec3 normal, vec3 position, vec3 viewDir)
{
    vec3 lightPosView = vec3(viewMatrix * vec4(pointLight.position, 1.0));
    vec3 lightDir = normalize(lightPosView - FSIn.positionViewspace);
    float distance = length(lightPosView - FSIn.positionViewspace);

    float radius = 20.0;
    float attenuation = clamp(1.0 - distance*distance/(radius*radius), 0.0, 1.0);
    attenuation *= attenuation;

    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 256.0);
    vec3 specular = spec * pointLight.color * attenuation * 0.8;

    return specular;
}

void main()
{
    // Reconstrucción de posición y vectores base
    vec3 Pw = vec3(viewMatrixInv * vec4(FSIn.positionViewspace, 1.0));
    vec2 texCoord = gl_FragCoord.xy / viewportSize;
    
    // Cálculo de normales
    vec2 normalMapCoord = Pw.xz * 0.1 / tileSize + vec2(time * 0.02);
    vec4 normalMapColour = texture(normalMap, normalMapCoord);
    vec3 tangentNormal = vec3(normalMapColour.r * 2.0 - 1.0, normalMapColour.g * 2.0 - 1.0, normalMapColour.b);
    tangentNormal = normalize(tangentNormal);
    
    // Crear matriz TBN
    vec3 N = normalize(FSIn.normalViewspace);
    vec3 T = normalize(cross(N, vec3(0.0, 1.0, 0.0)));
    vec3 B = cross(N, T);
    mat3 TBN = mat3(T, B, N);
    vec3 normal = normalize(TBN * tangentNormal);
    
    vec3 V = normalize(-FSIn.positionViewspace);

    // Cálculo de profundidad
    const float near = 0.1;
    const float far = 1000.0;
    
    float groundDepth = texture(refractionDepth, texCoord).r;
    float floorDistance = 2.0 * near * far / (far + near - (2.0 * groundDepth - 1.0) * (far - near));
    float waterSurfaceDistance = 2.0 * near * far / (far + near - (2.0 * gl_FragCoord.z - 1.0) * (far - near));
    float waterDepth = max(0.0, floorDistance - waterSurfaceDistance);
    
    float distortionFactor = clamp(waterDepth / turbidityDistance, 0.0, 1.0);

    // Distorsión
    vec2 distortion = (texture(dudvMap, Pw.xz * 0.1 / tileSize + vec2(time * 0.02)).rg * 2.0 - 1.0) * 0.05 * distortionFactor;
    
    // Coordenadas de textura
    vec2 reflectionTexCoord = clamp(texCoord + distortion, 0.001, 0.999);
    reflectionTexCoord.y = 1.0 - reflectionTexCoord.y;
    vec2 refractionTexCoord = texCoord + distortion;
    
    // Colores base
    vec3 reflectionColor = texture(reflectionMap, reflectionTexCoord).rgb;
    vec3 refractionColor = texture(refractionMap, refractionTexCoord).rgb;

    // Efecto de absorción
    float tintFactor = clamp(waterDepth / turbidityDistance, 0.0, 0.1);
    float absorption = clamp(waterDepth / turbidityDistance, 0.0, 0.5);
    refractionColor *= 1.0 - absorption;
    refractionColor = mix(refractionColor, waterColor, tintFactor);
    
    // Fresnel
    vec3 F0 = vec3(0.2);
    vec3 F = fresnelSchlick(max(0.0, dot(V, N)), F0);
    vec3 waterFinalColor = mix(refractionColor, reflectionColor, F);
    
    // Iluminación especular
    for(int i = 0; i < uLightCount; ++i)
    {
       if(uLight[i].type == 0) // Directional light
       {
            waterFinalColor += CalcDirLight(uLight[i], normal, V);
       }
       else if(uLight[i].type == 1) // Point light
       {
            waterFinalColor += CalcPointLight(uLight[i], normal, Pw, V);       
       }
    }

    // Espuma
    float foamAmount = 0.0;
    vec3 foamColor = vec3(0.9, 0.9, 1.0);

    if (waterDepth < FOAM_FALLOFF_DISTANCE) {
        float depthFactorFoam = 1.0 - clamp(waterDepth / FOAM_FALLOFF_DISTANCE, 0.0, 1.0);
        foamAmount = pow(depthFactorFoam, FOAM_FALLOFF_BIAS);
        
        vec2 foamUV = Pw.xz * FOAM_SCALE * FOAM_TILING + vec2(time * FOAM_SCROLL_SPEED);
        float channelA = texture(foamMap, foamUV - vec2(FOAM_SCROLL_SPEED, cos(foamUV.x))).r;
        float channelB = texture(foamMap, foamUV * 0.25 + vec2(sin(foamUV.y), FOAM_SCROLL_SPEED)).b;

        float foamRaw = (channelA + channelB) * 0.75;
        float foamPattern = smoothstep(0.35, 0.65, foamRaw);
        foamPattern = clamp(foamPattern, 0.0, 1.0);
        
        float foamMask = 1.0 - foamPattern;
        float leadingFactor = 1.0 - clamp(waterDepth / FOAM_LEADING_EDGE, 0.0, 1.0);
        foamAmount *= mix(foamMask, 1.0, leadingFactor);
    }
    
    waterFinalColor += foamColor * foamAmount;
   
    // Transparencia
    float alpha = clamp(waterDepth / 5.0, 0.0, 1.0);
    
    outColor = vec4(waterFinalColor, alpha);
}

#endif
#endif
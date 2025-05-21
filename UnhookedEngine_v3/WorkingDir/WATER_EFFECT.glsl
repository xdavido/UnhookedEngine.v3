#ifdef WATER_EFFECT

#if defined(VERTEX)
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

#elif defined(FRAGMENT)

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

struct Light {
    uint type;
    vec3 color;
    vec3 direction;
    vec3 position;
};

layout(binding = 0, std140) uniform GlobalParams {
    vec3 uCameraPosition;
    uint uLightCount;
    Light uLight[16];
};

in Data {
    vec3 positionViewspace;
    vec3 normalViewspace;
} FSIn;

out vec4 outColor;

// ===== LIGHT FUNCTIONS =====

vec3 CalcDirLight(Light alight, vec3 normal, vec3 viewDir)
{
    vec3 lightDir = normalize(-alight.direction);
    float diff = max(dot(normal, lightDir), 0.0);
    float angleFactor = pow(diff, 0.7);

    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);

    vec3 ambient = alight.color * 0.05 + (0.15 * diff) * alight.color;
    vec3 diffuse = angleFactor * alight.color * 0.5;
    vec3 specular = spec * alight.color * 0.8;

    return ambient + diffuse + specular;
}

vec3 CalcPointLight(Light pointLight, vec3 normal, vec3 position, vec3 viewDir)
{
    vec3 lightDir = normalize(pointLight.position - position);
    float distance = length(pointLight.position - position);
    float radius = 20.0;
    float attenuation = clamp(1.0 - distance / radius, 0.0, 1.0);
    attenuation *= attenuation;

    float diff = max(dot(normal, lightDir), 0.0);
    float angleFactor = pow(diff, 0.8);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 128.0);

    vec3 ambient = pointLight.color * 0.1;
    vec3 diffuse = angleFactor * pointLight.color * 0.6;
    vec3 specular = spec * pointLight.color * 2.5;

    return attenuation * (ambient + diffuse + specular);
}

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

// Foam parameters
const float FOAM_FALLOFF_DISTANCE = 3.0; // How far from intersection foam extends
const float FOAM_FALLOFF_BIAS = 0.8;     // Controls foam gradient (1.0 = hard edge)
const float FOAM_LEADING_EDGE = 1.0;     // Softens edge where foam meets object
const float FOAM_SCROLL_SPEED = 0.2;     // Speed of foam texture scrolling
const float FOAM_TILING = 1.0;           // How much to tile the foam texture
const float FOAM_SCALE = 0.1;            // Scale of foam texture

void main()
{
    vec3 V = normalize(-FSIn.positionViewspace);
    vec3 Pw = vec3(viewMatrixInv * vec4(FSIn.positionViewspace, 1.0));
    vec2 texCoord = gl_FragCoord.xy / viewportSize;

    // Distortion (solo para reflexión/refracción)
    vec2 distortion1 = (texture(dudvMap, Pw.xz * 0.1 / tileSize + vec2(time * 0.02)).rg * 2.0 - 1.0) * 0.05;
    vec2 distortion2 = (texture(dudvMap, Pw.xz * 0.1 / tileSize + vec2(time * 0.02, time * 0.01)).rg * 2.0 - 1.0) * 0.05;
    vec2 totalDistortion = distortion1 + distortion2;

    // Perturbed normal
    vec3 normalMapValue = texture(normalMap, Pw.xz * 0.1 / tileSize + totalDistortion).rgb;
    vec3 tangentNormal = normalize(normalMapValue * 2.0 - 1.0);

    // TBN
    vec3 N = normalize(FSIn.normalViewspace);
    vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
    vec3 T = normalize(cross(up, N));
    vec3 B = normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);
    vec3 perturbedNormal = normalize(TBN * tangentNormal);

    // Distorted UVs for reflection/refraction
    vec2 reflectionTexCoord = vec2(texCoord.s, 1.0 - texCoord.t) + totalDistortion;
    vec2 refractionTexCoord = texCoord + totalDistortion;

    // Reflection and refraction colors
    vec3 reflectionColor = texture(reflectionMap, reflectionTexCoord).rgb;
    vec3 refractionColor = texture(refractionMap, refractionTexCoord).rgb;

    // Depth with distortion (para tint, reflexión/refracción)
    float distortedGroundDepth = texture(refractionDepth, refractionTexCoord).x;
    vec3 distortedGroundPosViewSpace = reconstructPixelPosition(distortedGroundDepth);
    float distortedWaterDepth = max(0.0, FSIn.positionViewspace.z - distortedGroundPosViewSpace.z);

    // --- Foam con profundidad sin distorsión ---
    float undistortedGroundDepth = texture(refractionDepth, texCoord).x;
    vec3 undistortedGroundPosViewSpace = reconstructPixelPosition(undistortedGroundDepth);
    float undistortedWaterDepth = max(0.0, FSIn.positionViewspace.z - undistortedGroundPosViewSpace.z);

    // Tint
    float tintFactor = clamp(distortedWaterDepth / 10.0, 0.0, 1.0);
    vec3 waterColor = vec3(0.25, 0.4, 0.6);
    refractionColor = mix(refractionColor, waterColor, tintFactor);

    // ===== IMPROVED FOAM EFFECT =====
    float foamAmount = 0.0;
    vec3 foamColor = vec3(0.9, 0.95, 1.0);

    if (undistortedWaterDepth < FOAM_FALLOFF_DISTANCE) {
        float depthFactor = 1.0 - clamp(undistortedWaterDepth / FOAM_FALLOFF_DISTANCE, 0.0, 1.0);
        foamAmount = pow(depthFactor, FOAM_FALLOFF_BIAS);

        // Foam texture with scrolling and distortion (solo UV, no profundidad distorsionada)
        vec2 foamUV = Pw.xz * FOAM_SCALE * FOAM_TILING + vec2(time * FOAM_SCROLL_SPEED);
        float channelA = texture(foamMap, foamUV - vec2(FOAM_SCROLL_SPEED, cos(foamUV.x))).r;
        float channelB = texture(foamMap, foamUV * 0.5 + vec2(sin(foamUV.y), FOAM_SCROLL_SPEED)).b;

        float foamPattern = pow((channelA + channelB) * 0.95, 1.5);
        foamPattern = pow(foamPattern, 2.0);
        foamPattern = clamp(foamPattern, 0.0, 1.0);

        // **Invertimos la máscara para que la zona negra sea blanca visible**
        float foamMask = 1.0 - foamPattern;

        // Leading edge factor: blanco al contacto con el objeto
        float leadingFactor = 1.0 - clamp(undistortedWaterDepth / FOAM_LEADING_EDGE, 0.0, 1.0);

        // Combinación suave del patrón y el borde cercano
        float blendedFoam = mix(foamMask, 1.0, leadingFactor);

        // Resultado final de la espuma
        foamAmount *= blendedFoam;
    }

    // Fresnel
    vec3 F0 = vec3(0.1);
    float fresnelFactor = max(dot(perturbedNormal, V), 0.0);
    vec3 F = fresnelSchlick(fresnelFactor, F0);

    // Lighting
    vec3 lightResult = vec3(0.0);
    for (int i = 0; i < int(uLightCount); ++i) {
        if (uLight[i].type == 0)
            lightResult += CalcDirLight(uLight[i], perturbedNormal, V);
        else if (uLight[i].type == 1)
            lightResult += CalcPointLight(uLight[i], perturbedNormal, Pw, V);
    }

    // Final water color
    vec3 waterFinalColor = mix(refractionColor, reflectionColor, F) + (lightResult * 2.0);

    // Add foam on top
    waterFinalColor += foamColor * foamAmount;

    outColor.rgb = waterFinalColor;
    outColor.a = 1.0;
}

#endif
#endif

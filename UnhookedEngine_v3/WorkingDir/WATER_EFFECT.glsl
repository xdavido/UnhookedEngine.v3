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

const float FOAM_FALLOFF_DISTANCE = 1.0;
const float FOAM_FALLOFF_BIAS = 2.0;
const float FOAM_LEADING_EDGE = 0.5;
const float FOAM_SCROLL_SPEED = 0.15;
const float FOAM_TILING = 3.0;
const float FOAM_SCALE = 0.15;

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

void main()
{
    vec3 N = normalize(FSIn.normalViewspace);
    vec3 V = normalize(-FSIn.positionViewspace);
    vec3 Pw = vec3(viewMatrixInv * vec4(FSIn.positionViewspace, 1.0));
    vec2 texCoord = gl_FragCoord.xy / viewportSize;

    const vec2 waveStrength = vec2(0.05);
    const float turbidityDistance = 1.0;

    vec2 distortion = (texture(dudvMap, Pw.xz * 0.1 / tileSize + vec2(time * 0.02)).rg * 2.0 - 1.0) * waveStrength;

    // Coordenadas de textura con distorsión (clamp para evitar huecos negros)
    vec2 reflectionTexCoord = clamp(vec2(texCoord.s, 1.0 - texCoord.t) + distortion, 0.001, 0.999);
    vec2 refractionTexCoord = clamp(texCoord + distortion, 0.001, 0.999);

    // Colores de reflexión y refracción
    vec3 reflectionColor = texture(reflectionMap, reflectionTexCoord).rgb;
    vec3 refractionColor = texture(refractionMap, refractionTexCoord).rgb;

    // Tinte del agua
    float distortedGroundDepth = texture(refractionDepth, refractionTexCoord).x;


    vec3 distortedGroundPosViewSpace = reconstructPixelPosition(distortedGroundDepth);
    float distortedWaterDepth = FSIn.positionViewspace.z - distortedGroundPosViewSpace.z;

    float tintFactor = clamp(distortedWaterDepth / turbidityDistance, 0.0, 0.1);
    vec3 waterColor = vec3(0.25, 0.4, 0.6);
    refractionColor = mix(refractionColor, waterColor, tintFactor);

    // Fresnel
    vec3 F0 = vec3(0.01);
    vec3 F = fresnelSchlick(max(0.0, dot(V, N)), F0);
    vec3 waterFinalColor = mix(refractionColor, reflectionColor, F);

 
    float foamAmount = 0.0;
    vec3 foamColor = vec3(0.9, 0.9, 1.0);

    float undistortedGroundDepth = texture(refractionDepth, texCoord).x;
    vec3 undistortedGroundPosViewSpace = reconstructPixelPosition(undistortedGroundDepth);
    float undistortedWaterDepth = max(0.0, FSIn.positionViewspace.z - undistortedGroundPosViewSpace.z);

    if (undistortedWaterDepth < FOAM_FALLOFF_DISTANCE) {
        float depthFactor = 1.0 - clamp(undistortedWaterDepth / FOAM_FALLOFF_DISTANCE, 0.0, 1.0);
        foamAmount = pow(depthFactor, FOAM_FALLOFF_BIAS);

        vec2 foamUV = Pw.xz * FOAM_SCALE * FOAM_TILING + vec2(time * FOAM_SCROLL_SPEED);
        float channelA = texture(foamMap, foamUV - vec2(FOAM_SCROLL_SPEED, cos(foamUV.x))).r;
        float channelB = texture(foamMap, foamUV * 0.5 + vec2(sin(foamUV.y), FOAM_SCROLL_SPEED)).b;

        float foamPattern = pow((channelA + channelB) * 0.95, 1.5);
        foamPattern = clamp(foamPattern, 0.0, 0.7);

        float foamMask = 1.0 - foamPattern;
        float leadingFactor = 1.0 - clamp(undistortedWaterDepth / FOAM_LEADING_EDGE, 0.0, 1.0);
        foamAmount *= mix(foamMask, 1.0, leadingFactor);
    }

    waterFinalColor += foamColor * foamAmount;

    outColor.rgb = waterFinalColor;
    outColor.a = 1.0;
}


#endif
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef WATER
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

uniform vec2 viewportSize;
uniform float time;  // Para animaci?n
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

void main()
{
    vec3 N = normalize(FSIn.normalViewspace);
    vec3 V = normalize(-FSIn.positionViewspace);
    vec3 Pw = vec3(viewMatrixInv * vec4(FSIn.positionViewspace, 1.0)); // position in world space
    vec2 texCoord = gl_FragCoord.xy / viewportSize;

    const vec2 waveLength = vec2(1.0);
    const vec2 waveStrength = vec2(0.05);
    const float turbidityDistance = 1.0;

    vec2 distortion1 = (texture(dudvMap, Pw.xz * 0.1 / tileSize + vec2(time * 0.02)).rg * 2.0 - 1.0) * waveStrength;
    vec2 distortion2 = (texture(dudvMap, Pw.xz * 0.1 / tileSize + vec2(time * 0.02, time * 0.01)).rg * 2.0 - 1.0) * waveStrength;

    vec2 distortion = distortion1 + distortion2;

   //vec2 distortion = (2.0 * texture(dudvMap, Pw.xz / waveLength).rg - vec2(1.0)) * waveStrength + waveStrength / 7.0;

    // Distorted reflection and refraction
    vec2 reflectionTexCoord = vec2(texCoord.s, 1.0 - texCoord.t) + distortion;
    vec2 refractionTexCoord = texCoord + distortion;
    vec3 reflectionColor = texture(reflectionMap, reflectionTexCoord).rgb;
    vec3 refractionColor = texture(refractionMap, refractionTexCoord).rgb;

    // Water tint
    float distortedGroundDepth = texture(refractionDepth, refractionTexCoord).x;
    vec3 distortedGroundPosViewSpace = reconstructPixelPosition(distortedGroundDepth);
    float distortedWaterDepth = FSIn.positionViewspace.z - distortedGroundPosViewSpace.z;


    float tintFactor = clamp(distortedWaterDepth / turbidityDistance, 0.0, 0.1);
    vec3 waterColor = vec3(0.25, 0.4, 0.6);
    refractionColor = mix(refractionColor, waterColor, tintFactor);

    // Fresnel
    vec3 F0 = vec3(0.01);
    vec3 F = fresnelSchlick(max(0.0, dot(V, N)), F0);
    outColor.rgb = mix(refractionColor, reflectionColor, F);
   
    outColor.a = 0.70; // Adjusted for transparency
}

#endif
#endif
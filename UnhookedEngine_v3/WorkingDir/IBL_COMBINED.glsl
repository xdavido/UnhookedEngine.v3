#ifdef IBL_COMBINED
#if defined(VERTEX)

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform vec3 uCameraPosition;

out vec3 vWorldNormal;
out vec3 vWorldPos;
out vec3 vViewDir;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;
    vWorldNormal = mat3(uModel) * aNormal;
    vViewDir = uCameraPosition - vWorldPos;
    gl_Position = uProjection * uView * worldPos;
}
#endif

#if defined(FRAGMENT)

in vec3 vWorldNormal;
in vec3 vWorldPos;
in vec3 vViewDir;

uniform samplerCube irradianceMap;
uniform samplerCube skyboxMap;

uniform vec3 baseColor = vec3(1.0, 0.0, 1.0);
uniform float reflectionStrength = 1.;

layout(location = 0) out vec4 oColor;

void main()
{
    vec3 N = normalize(vWorldNormal);
    vec3 V = normalize(vViewDir);
    if (length(V) < 0.0001) discard;
    if (length(N) < 0.0001) discard;
    vec3 R = reflect(-V, N);

    vec3 diffuse = texture(irradianceMap, N).rgb * baseColor;
    vec3 specular = texture(skyboxMap, R).rgb;

    vec3 color = diffuse + reflectionStrength * specular;
    oColor = vec4(color, 1.0);
}
#endif
#endif


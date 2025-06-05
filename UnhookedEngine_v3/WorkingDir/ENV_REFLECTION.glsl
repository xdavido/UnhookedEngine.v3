#ifdef ENV_REFLECTION
#if defined(VERTEX)

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vNormal;
out vec3 vWorldPos;

void main()
{
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;
    vNormal = mat3(transpose(inverse(uModel))) * aNormal;
    gl_Position = uProjection * uView * worldPos;
}

#elif defined(FRAGMENT)

in vec3 vNormal;
in vec3 vWorldPos;

uniform vec3 uCameraPosition;
uniform samplerCube skybox;

out vec4 FragColor;

void main()
{
    vec3 I = normalize(vWorldPos - uCameraPosition); // vector de vista
    vec3 N = normalize(vNormal);
    vec3 R = reflect(I, N); // reflexión

    vec3 reflectedColor = texture(skybox, R).rgb;
    FragColor = vec4(reflectedColor, 1.0);
}

#endif
#endif

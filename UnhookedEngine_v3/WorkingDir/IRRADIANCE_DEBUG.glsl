#ifdef IRRADIANCE_DEBUG
#if defined(VERTEX)

layout(location = 0) in vec3 aPosition;

out vec3 vLocalPos;

uniform mat4 view;
uniform mat4 projection;

void main()
{
    vLocalPos = aPosition;
    gl_Position = projection * view * vec4(aPosition, 1.0);
}

#endif

#if defined(FRAGMENT)

in vec3 vLocalPos;
out vec4 FragColor;

uniform samplerCube irradianceMap;

void main()
{
    vec3 dir = normalize(vLocalPos);
    vec3 color = texture(irradianceMap, dir).rgb;
    FragColor = vec4(color, 1.0);
}

#endif
#endif

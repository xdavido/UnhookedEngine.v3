#ifdef IBL_CONVOLUTION
#if defined(VERTEX)

layout(location = 0) in vec3 aPos;

uniform mat4 projection;
uniform mat4 view;

out vec3 localPos;

void main()
{
    localPos = aPos;
    gl_Position = projection * view * vec4(aPos, 1.0);
}

#endif

#if defined(FRAGMENT)

in vec3 localPos;

uniform samplerCube environmentMap;

const float PI = 3.14159265359;

out vec4 FragColor;

void main()
{
    vec3 N = normalize(localPos);
    vec3 irradiance = vec3(0.0);

    vec3 up = abs(N.y) > 0.999 ? vec3(0, 0, 1) : vec3(0, 1, 0);
    vec3 right = normalize(cross(up, N));
    up = normalize(cross(N, right));

    float sampleDelta = .1;
    float nrSamples = 1.0;

    for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta)
    {
        for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta)
        {
            vec3 tangentSample = vec3(
                sin(theta) * cos(phi),
                sin(theta) * sin(phi),
                cos(theta)
            );

            vec3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * N;

            irradiance += texture(environmentMap, sampleVec).rgb * cos(theta) * sin(theta);
            nrSamples += 1.0; 
        }
    }

    irradiance = PI * irradiance * (1.0 / float(nrSamples));
    FragColor = vec4(irradiance, 1.0);
}

#endif
#endif

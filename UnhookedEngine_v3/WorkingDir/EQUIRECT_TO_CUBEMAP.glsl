#ifdef EQUIRECT_TO_CUBEMAP
    #if defined(VERTEX)

    layout(location = 0) in vec3 aPos;
    out vec3 localPos;

    uniform mat4 projection;
    uniform mat4 view;

    void main() {
        localPos = aPos;
        gl_Position = projection * view * vec4(aPos, 1.0);
    }

    #endif

    #ifdef FRAGMENT

    in vec3 localPos;
    out vec4 FragColor;

    uniform sampler2D equirectangularMap;
    const vec2 invAtan = vec2(0.1591, 0.3183);

    vec2 SampleSphericalMap(vec3 v) {
        vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
        uv *= invAtan;
        uv += 0.5;
        return uv;
    }

    void main() {
        vec3 dir = normalize(localPos);
        vec2 uv = SampleSphericalMap(dir);
        vec3 color = texture(equirectangularMap, uv).rgb;
        FragColor = vec4(color, 1.0);
    }

#endif
#endif

////////////////////////////////////
#ifdef USE_TONEMAP
#if defined(VERTEX)

layout(location = 0) in vec3 aPos;
out vec3 localPos;

uniform mat4 projection;
uniform mat4 view;

void main()
{
    localPos = aPos;
    gl_Position = projection * view * vec4(aPos, 1.0);
}
#elif defined(FRAGMENT) ////////////////////////////////////////


in vec3 localPos;
out vec4 FragColor;

uniform sampler2D equirectangularMap;
const vec2 invAtan = vec2(0.1591, 0.3183);

// Tone mapping parameters
uniform float exposure = 1.0;

vec2 SampleSphericalMap(vec3 v) {
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

// Simple exponential tone mapping
vec3 ToneMap(vec3 color) {
    // Exposure tone mapping
    color = vec3(1.0) - exp(-color * exposure);

    // Gamma correction (assuming gamma 2.2)
    color = pow(color, vec3(1.0 / 2.2));

    return color;
}

void main() {
    vec3 dir = normalize(localPos);
    vec2 uv = SampleSphericalMap(dir);
    vec3 hdrColor = texture(equirectangularMap, uv).rgb;

    vec3 toneMapped = ToneMap(hdrColor);
    FragColor = vec4(toneMapped, 1.0);
}

#endif
#endif


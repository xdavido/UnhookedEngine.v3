#ifdef SKYBOX

#if defined(VERTEX) ////////////////////////////////////////
layout(location = 0) in vec3 aPos;
out vec3 TexCoords;

uniform mat4 projection;
uniform mat4 view;

void main()
{
    TexCoords = aPos;
    vec4 pos = projection * mat4(mat3(view)) * vec4(aPos, 1.0);
    gl_Position = pos.xyww;
}

#elif defined(FRAGMENT) ////////////////////////////////////////

in vec3 TexCoords;
out vec4 FragColor;

uniform samplerCube skybox;

void main()
{
    vec3 dir = normalize(TexCoords);
    FragColor = texture(skybox, dir);
}
#endif

#endif




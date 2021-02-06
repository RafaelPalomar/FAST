#version 130
in vec3 aPos;
in vec2 aTexCoord;

out vec2 TexCoord;

uniform mat4 transform;
uniform mat4 viewTransform;
uniform mat4 perspectiveTransform;

void main()
{
    gl_Position = perspectiveTransform * viewTransform * transform * vec4(aPos, 1.0);
    TexCoord = aTexCoord;
}

#version 330
in vec3 aPos;
out vec3 TexCoords;

uniform mat4 projectionMatrix;
uniform mat4 viewMatrix;

void main()
{
    TexCoords = aPos;
    gl_Position = projectionMatrix * viewMatrix * vec4(aPos, 1.0);
}  

#version 330
in vec3 vertex;
in vec3 color;
in vec2 uv;

uniform mat4 projectionMatrix;
uniform mat4 viewMatrix;
uniform mat4 transformMatrix;

out vec3 fragColor;
out vec2 texCord;

void main()
{
    gl_Position = projectionMatrix * viewMatrix * transformMatrix * vec4(vertex,1.0);
    fragColor = color;
    texCord = uv;
}

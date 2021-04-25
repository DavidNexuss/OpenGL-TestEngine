
#version 330
in vec3 vertex;
in vec3 color;
in vec2 uv;
in vec3 normal;

uniform mat4 projectionMatrix;
uniform mat4 viewMatrix;
uniform mat4 transformMatrix;
uniform mat3 normalMatrix;

out vec3 fragColor;
out vec4 fragPosition;
out vec2 texCord;
out vec3 normalCord;

void main()
{
    fragPosition = transformMatrix * vec4(vertex,1.0);
    gl_Position = projectionMatrix * viewMatrix * fragPosition;
    fragColor = color;
    texCord = uv;
    normalCord = normalMatrix * normal;
}

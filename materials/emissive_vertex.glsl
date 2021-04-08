#version 330
in vec3 vertex;
in vec3 color;

uniform mat4 projectionMatrix;
uniform mat4 viewMatrix;
uniform mat4 transformMatrix;

out vec3 fragColor;

void main()
{
    gl_Position = projectionMatrix * viewMatrix * transformMatrix * vec4(vertex,1.0);
    fragColor = color;
}


#version 330
in vec3 vertex;

uniform mat4 projectionMatrix;
uniform mat4 viewMatrix;
uniform mat4 transformMatrix;

void main()
{
    gl_Position = projectionMatrix * viewMatrix * transformMatrix * vec4(vertex,1.0);
}

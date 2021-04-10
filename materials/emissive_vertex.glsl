#version 330
in vec3 vertex;
in vec3 color;

uniform mat4 projectionMatrix;
uniform mat4 viewMatrix;
uniform mat4 transformMatrix;
uniform float time;

out vec3 fragColor;

void main()
{
    gl_Position = projectionMatrix * viewMatrix * transformMatrix * vec4(vertex,1.0);
    fragColor = color;
    fragColor.xy = fragColor.xy * (sin(time + vertex.x)*0.5 + 0.5);
    fragColor.yz = fragColor.yz * (cos(time + vertex.y)*0.5 + 0.5);
}

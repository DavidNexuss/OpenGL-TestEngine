#version 330
out vec3 color;
in vec3 fragColor;
uniform float time;
uniform vec3 emissive;
uniform float factor;

void main()
{
    float a = factor;
    color = emissive * fragColor * ((sin(time) * factor) + factor);
}

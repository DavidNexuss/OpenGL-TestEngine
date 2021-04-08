#version 330
out vec3 color;
in vec3 fragColor;
uniform vec3 emissive;
uniform float time;

void main()
{
    float a = 0.7;
    color = emissive * (1.0 + (sin(time + gl_FragCoord.x * 0.001) * a + a));
}

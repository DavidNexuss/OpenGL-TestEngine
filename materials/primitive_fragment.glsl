#version 330
out vec3 color;
in vec3 fragColor;
void main()
{
    color = fragColor;
}

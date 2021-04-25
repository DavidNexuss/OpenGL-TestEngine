#version 330
out vec4 color;
in vec3 fragColor;
in vec2 texCord;
in vec3 normalCord;

uniform sampler2D texture0;

void main()
{
    color = texture(texture0,texCord) * vec4(abs(normalCord),1.0);
}
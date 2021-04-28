#version 330
out vec4 color;
in vec3 TexCoords;
uniform samplerCube texture0;

void main()
{    
    color = texture(texture0, TexCoords);
}
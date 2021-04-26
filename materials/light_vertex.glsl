
#version 330
in vec3 vertex;
in vec3 color;
in vec2 uv;
in vec3 normal;
in vec3 aTangent;
in vec3 aBitangent;

uniform mat4 projectionMatrix;
uniform mat4 viewMatrix;
uniform mat4 transformMatrix;
uniform mat3 normalMatrix;

out vec3 fragColor;
out vec4 fragPosition;
out vec2 texCord;
out vec3 normalCord;
out mat3 TBN;
void main()
{
    fragPosition = transformMatrix * vec4(vertex,1.0);
    gl_Position = projectionMatrix * viewMatrix * fragPosition;
    fragColor = color;
    texCord = uv;
    normalCord = normalMatrix * normal;
    vec3 T = normalize(vec3(transformMatrix * vec4(aTangent,   0.0)));
    vec3 N = normalize(vec3(transformMatrix * vec4(normal,    0.0)));
    vec3 B = normalize(cross(N,T)); //normalize(vec3(transformMatrix * vec4(aBitangent, 0.0)));
    TBN = mat3(T, B, N);
}

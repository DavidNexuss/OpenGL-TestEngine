
#version 330
in vec3 aVertex;
in vec3 aColor;
in vec2 aUv;
in vec3 aNormal;
in vec3 aTangent;

uniform mat4 projectionMatrix;
uniform mat4 viewMatrix;
uniform mat4 transformMatrix;
uniform mat3 normalMatrix;

out vec3 fragColor;
out vec4 fragPosition;
out vec2 texCoord;
out vec3 normalCoord;
out mat3 TBN;
void main()
{
    fragPosition = transformMatrix * vec4(aVertex,1.0);
    gl_Position = projectionMatrix * viewMatrix * fragPosition;
    fragColor = aColor;
    texCoord = aUv;
    normalCoord = normalMatrix * aNormal;
    vec3 T = normalize(vec3(transformMatrix * vec4(aTangent,   0.0)));
    vec3 N = normalize(vec3(transformMatrix * vec4(aNormal,    0.0)));
    vec3 B = normalize(cross(N,T));
    TBN = mat3(T, B, N);
}

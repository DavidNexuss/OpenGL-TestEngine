#define _GLIBCXX_PARALLEL

#include "window.h"
#include "load_shader.h"
#include <iostream>
#include <vector>
#include <map>
#include <list>
#include <map>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <memory>
#include <imgui.h>
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "spatial.h"

using namespace std;
using namespace glm;

const float deltaTime = 0.1;


/*
 * This templates keeps a sorted vector and a set of unordered references mapped to the vector
 * Bheaviour is the same as vector except that this structure enables for an ordered trasversal of the items
 */

#define BATCHING_ENABLED
template <typename T>
struct sorted_vector
{
    vector<T> values;
    vector<size_t> indexs;
    map<T,size_t> batch;

    sorted_vector() { }

    size_t push_back(T value)
    {
        #ifdef BATCHING_ENABLED
            auto it = lower_bound(values.begin(), values.end(),value);
            size_t a = it - values.begin();
            it = values.insert(it, value);

            for (int i = a; i < indexs.size(); i++)
            {
                indexs[i]++;
            }
            indexs.push_back(a);
            batch[value]++;
            return indexs.size() - 1;

        #else
            values.push_back(value);
            indexs.push_back(values.size() - 1);
            return indexs.size() - 1;
        #endif
    }

    const T& operator[] (size_t idx) const { return values[indexs[idx]]; }
    T& operator[] (size_t idx) { return values[indexs[idx]]; }

    inline bool size() { return indexs.size(); } 
    vector<T>& native() {return values; }
};

namespace Debug
{
    int materialSwaps;
    int materialInstanceSwaps;
    int meshSwaps;
    int textureSwaps;
    int uniformsFlush;
    int lightFlush;

    int missingUniforms;

    inline void reset()
    {
        materialSwaps = 0;
        materialInstanceSwaps = 0;
        meshSwaps = 0;
        textureSwaps = 0;
        uniformsFlush = 0;
        lightFlush = 0;
    }

    inline void print()
    {
        cerr << "Frame debug information: " << endl;
        cerr << "Material swaps :" << materialSwaps << endl;
        cerr << "Matrial instance swap: " << materialInstanceSwaps << endl;
        cerr << "Uniform flushs: " << uniformsFlush << endl;
        cerr << "Light flush: " << lightFlush << endl;
        cerr << "Mesh swaps :" << meshSwaps << endl;
        cerr << "Texture swaps :" << textureSwaps << endl;
        cerr << "----" << endl;
        cerr << "Missing uniforms: " << missingUniforms << endl;
        cerr << "----" << endl;
    }
}
#define DEBUG
#ifdef DEBUG 
    #define REGISTER_MISSED_UNIFORM() Debug::missingUniforms++
    #define REGISTER_FRAME() Debug::reset()
    #define REGISTER_MATERIAL_SWAP() Debug::materialSwaps++
    #define REGISTER_MESH_SWAP() Debug::meshSwaps++
    #define REGISTER_TEXTURE_SWAP() Debug::textureSwaps++
    #define REGISTER_MATERIAL_INSTANCE_SWAP() Debug::materialInstanceSwaps++
    #define REGISTER_UNIFORM_FLUSH() Debug::uniformsFlush++
    #define REGISTER_LIGHT_FLUSH() Debug::lightFlush++
    #define LOG_FRAME() Debug::print()    
#else
    #define REGISTER_MISSED_UNIFORM()
    #define REGISTER_FRAME()
    #define REGISTER_MATERIAL_SWAP()
    #define REGISTER_MESH_SWAP()
    #define REGISTER_TEXTURE_SWAP()
    #define REGISTER_MATERIAL_INSTANCE_SWAP()
    #define REGISTER_UNIFORM_FLUSH()
    #define REGISTER_LIGHT_FLUSH()
    #define LOG_FRAME()
#endif

namespace Directory
{
    const string texturePrefix = "textures/";
    const string materialPrefix = "materials/";
}

/**
 * Mantiene todos los atributos del viewport y de glfw a procesar,incluido el input
 */
namespace Viewport
{
    double screenWidth = 800,screenHeight = 600;
    double xpos,ypos;
    double scrollX,scrollY;

    void cursor_position_callback(GLFWwindow* window, double x, double y)
    {
        xpos = x;
        ypos = y;
    }
    
    void framebuffer_size_callback(GLFWwindow* window, int width, int height)
    {
        screenWidth = width;
        screenHeight = height;
        glViewport(0, 0, width, height);
    }
    
    void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
    {
        scrollX = xoffset;
        scrollY = yoffset;
    }
}

struct TextureData
{
    int width, height, nrChannels;
    unsigned char* data;

    TextureData(const string& path)
    {
        data = stbi_load((Directory::texturePrefix + path).c_str(),&width,&height,&nrChannels,0);
        if (!data)
        {
            cerr << "Error loading texture " << path << " from disk" << endl;
            throw std::runtime_error("Error loading texture");
        }
    }
};
using TextureID = size_t;
namespace Texture
{
    const static size_t maxTextureUnits = 16;
    vector<TextureData> texturesData;                   // textureID -> textureData
    vector<GLuint> glTexturesIds;                       // textureID -> GLID
    vector<TextureID> texturesUnits(maxTextureUnits,-1);             // slot -> textureID

    TextureID loadTexture(const TextureData& textureData)
    {
        if (!textureData.data ) return -1;

        GLuint texId;
        glGenTextures(1,&texId);
        glBindTexture(GL_TEXTURE_2D,texId);

        //Texture filtering
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        GLint format = textureData.nrChannels == 3 ? GL_RGB : GL_RGBA; 
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, textureData.width, textureData.height, 0, format, GL_UNSIGNED_BYTE, textureData.data);
        glGenerateMipmap(GL_TEXTURE_2D);

        texturesData.push_back(textureData);
        glTexturesIds.push_back(texId);
        return texturesData.size() - 1;
    }

    inline void useTexture(TextureID textureID,int textureUnit)
    {
        GLuint glTextureID = glTexturesIds[textureID];
        if (texturesUnits[textureUnit] != textureID)
        {
            texturesUnits[textureUnit] = textureID;
            glActiveTexture(GL_TEXTURE0 + textureUnit);
            glBindTexture(GL_TEXTURE_2D,glTextureID);
            REGISTER_TEXTURE_SWAP();
        }
    }
}

#define UNIFORMS_LIST(o) \
    o(VEC2,glm::vec2) o(VEC3,glm::vec3) o(VEC4,glm::vec4) \
    o(MAT2,glm::mat2) o(MAT3,glm::mat3) o(MAT4,glm::mat4) \
    o(FLOAT,float) o(BOOL,bool) o(INT,int)

enum UniformType
{
    #define UNIFORMS_ENUMS_DECLARATION(v,T) v ,
    UNIFORMS_LIST(UNIFORMS_ENUMS_DECLARATION)
    #undef UNIFORMS_ENUMS_DECLARATION
};

using UniformID = size_t;
class Uniform
{
    public:
    union
    {
        #define UNIFORMS_UNION_DECLARATION(v,T) T v;
        UNIFORMS_LIST(UNIFORMS_UNION_DECLARATION)
        #undef UNIFORMS_UNION_DECLARATION
    };

    UniformType type;
    bool forward;

    Uniform() { }
    #define UNIFORMS_CONSTRUCTOR(v,T) constexpr Uniform(const T& _##v) : v(_##v),type(UniformType::v),forward(true) { }
    UNIFORMS_LIST(UNIFORMS_CONSTRUCTOR)
    #undef UNIFORMS_CONSTRUCTOR
};

using MaterialInstanceID = size_t;
struct MaterialInstance
{
    vector<Uniform> uniformValues;
    vector<TextureID> assignedTextureUnits;

    MaterialInstance(size_t size = 0) : uniformValues(size), 
    assignedTextureUnits(Texture::maxTextureUnits,-1) { }

    MaterialInstance(const vector<Uniform>& _uniformValues) : uniformValues(_uniformValues), 
    assignedTextureUnits(Texture::maxTextureUnits,-1)
    {}

    inline void useUniform(UniformID id,GLuint glUniformID)
    {
        uniformValues[id].forward = false;
        const Uniform& current = uniformValues[id];

        switch(current.type)
        {
            case UniformType::VEC2:
            glUniform2fv(glUniformID,1,&current.VEC2[0]); break;
            case UniformType::VEC3:
            glUniform3fv(glUniformID,1,&current.VEC3[0]); break;
            case UniformType::VEC4:
            glUniform4fv(glUniformID,1,&current.VEC4[0]); break;
            case UniformType::MAT2:
            glUniformMatrix2fv(glUniformID,1,false,&current.MAT2[0][0]); break;
            case UniformType::MAT3:
            glUniformMatrix3fv(glUniformID,1,false,&current.MAT3[0][0]); break;
            case UniformType::MAT4:
            glUniformMatrix4fv(glUniformID,1,false,&current.MAT4[0][0]); break;
            case UniformType::FLOAT:
            glUniform1f(glUniformID,current.FLOAT); break;
            case UniformType::BOOL:
            break;
        }
        REGISTER_UNIFORM_FLUSH();
    }
    #define UNIFORMS_FUNC_DECLARATION(v,T) \
    inline void set(UniformID id,const T& a) { uniformValues[id].v = a; uniformValues[id].forward = true; uniformValues[id].type = UniformType::v; }

    UNIFORMS_LIST(UNIFORMS_FUNC_DECLARATION)
    #undef UNIFORMS_FUNC_DECLARATION

    inline void setTexture(TextureID textureID,int unitID) {
        assignedTextureUnits[unitID] = textureID;
    }
};

namespace MaterialInstanceLoader
{
    vector<MaterialInstance> materialInstances;

    MaterialInstanceID loadMaterialInstance(const MaterialInstance& materialInstance)
    {
        materialInstances.push_back(materialInstance);
        return materialInstances.size() - 1;
    }
}

enum UniformBasics
{
    UNIFORM_PROJECTION_MATRIX = 0,
    UNIFORM_VIEW_MATRIX,
    UNIFORM_TRANSFORM_MATRIX,
    UNIFORM_NORMAL_MATRIX,
    UNIFORM_TIME,
    UNIFORM_LIGHTCOLOR,
    UNIFORM_LIGHTPOSITION,
    UNIFORM_LIGHTCOUNT,
    UNIFORM_VIEW_POS,
    UNIFORM_COUNT
};

using MaterialID = size_t;
struct Material
{
    const static int scene_uniform_count = UNIFORM_COUNT;
    
    GLuint programID;
    vector<GLuint> uniforms;
    vector<MaterialInstanceID> usedInstances;
    
    string materialName;
    vector<GLuint> textureUniforms;

    Material(string materialName,const list<string>& uniforms)
    {
        this->materialName = materialName;
        string fragmentPath = Directory::materialPrefix + materialName + "_fragment.glsl";
        string vertexPath = Directory::materialPrefix + materialName + "_vertex.glsl";
    
        programID = compileShader(vertexPath.c_str(),fragmentPath.c_str());
        
        if (programID == -1)
        {
            cerr << "Error loading shaders!!!" << endl;
            throw std::runtime_error("Error compiling shader");
        }

        loadShaderUniforms(uniforms);
    }

    inline GLuint getUniformLocation(GLuint programID,const char* name) const
    {
        GLuint location = glGetUniformLocation(programID,name);
        if (location == -1)
        {
            cerr << "Missing Uniform Location! " << programID << " " << materialName << " -> " << name << endl;
            REGISTER_MISSED_UNIFORM();
        }
        return location;
    }
    void loadShaderUniforms(const list<string>& uniformsList)
    { 
        for(int i = 0; i < scene_uniform_count; i++)
            uniforms.push_back(-1);

        uniforms[UNIFORM_PROJECTION_MATRIX] = getUniformLocation(programID,"projectionMatrix");
        uniforms[UNIFORM_VIEW_MATRIX] = getUniformLocation(programID,"viewMatrix");
        uniforms[UNIFORM_TRANSFORM_MATRIX] = getUniformLocation(programID,"transformMatrix");
        uniforms[UNIFORM_NORMAL_MATRIX] = getUniformLocation(programID,"normalMatrix");
        uniforms[UNIFORM_TIME] = getUniformLocation(programID,"time");
        uniforms[UNIFORM_LIGHTCOLOR] = getUniformLocation(programID,"lightColor");
        uniforms[UNIFORM_LIGHTPOSITION] = getUniformLocation(programID,"lightPosition");
        uniforms[UNIFORM_LIGHTCOUNT] = getUniformLocation(programID,"lightCount");
        uniforms[UNIFORM_VIEW_POS] = getUniformLocation(programID,"viewPos");

        for(const auto& uniform : uniformsList)
        {
            uniforms.emplace_back(getUniformLocation(programID,uniform.c_str()));
            usedInstances.emplace_back();
            //TODO: check uniform not found
        }

        GLuint location = 0;
        for (size_t i = 0; i < Texture::maxTextureUnits and location != -1; i++)
        {
            string uniformName = "texture" + to_string(i);
            location = glGetUniformLocation(programID,uniformName.c_str());
            if(location != -1)
            {
                textureUniforms.push_back(location);
            }
        }
        
    }

    inline void bind()
    {
        glUseProgram(programID);
    }

    void useInstance(MaterialInstanceID materialInstanceID)
    {
        bool swap = false;
        const MaterialInstance& instance = MaterialInstanceLoader::materialInstances[materialInstanceID];

        // Set all uniforms
        for (UniformID i = 0; i < usedInstances.size(); i++)
        {
            if (usedInstances[i] != materialInstanceID || instance.uniformValues[i].forward)
            {
                usedInstances[i] = materialInstanceID;
                MaterialInstanceLoader::materialInstances[materialInstanceID].useUniform(i,uniforms[i + scene_uniform_count]);
                swap = true;
            }
        }

        for (size_t i = 0; i < instance.assignedTextureUnits.size(); i++)
        {
            if (instance.assignedTextureUnits[i] != -1)
            {
                Texture::useTexture(instance.assignedTextureUnits[i],i);
                glUniform1i(textureUniforms[i],i);
            }
        }
        
        if (swap) REGISTER_MATERIAL_INSTANCE_SWAP();
    }

    inline bool isLightSensitive() const
    {
        return uniforms[UNIFORM_LIGHTPOSITION] != -1;
    }
};

/** 
 * Mantiene todos los shaders cargados i crea la abstracción de Material
 */
namespace MaterialLoader
{
    MaterialID debugMaterialID = -1;
    MaterialInstanceID debugMaterialInstanceID = -1;
    
    vector<Material> materials;
    vector<size_t> usedMaterials;

    MaterialID currentMaterial = -1;
    
    MaterialID loadMaterial(const Material& material)
    {
        materials.push_back(material);
        usedMaterials.push_back(0);
        MaterialID mat = materials.size() - 1;
        return mat;
    }

    const inline vector<GLuint>& current()
    {
        return materials[currentMaterial].uniforms;
    }
};

enum CameraType
{
    THIRDPERSON = 0,
    THIRDPERSON_MANUAL,
    ORTHOGONAL,
    FLY,
    CUSTOM
};

const float zoomDamping = 0.6;

struct Camera
{
    CameraType type;
    
    glm::vec3 focusOrigin;
    glm::mat4 projectionMatrix;
    glm::mat4 viewMatrix;
    glm::mat4 invViewMatrix;

    float zoomFactor;
    float zoomSpeed;
    float fov;
    float phi,zheta,gamma;       //For manual controlling
    float d;
    float l,r,b,t,zmin,zmax;

    Camera()
    {
        fov = 90.0f;
        focusOrigin = vec3(0,0,0);
        l = b = -1.0f;
        r = t = 1.0f;
        zmin = -2.0;
        zmax = 2.0;
        zoomSpeed = 0.0;
        zoomFactor = 1.0;
        d = 5.0;
        type = THIRDPERSON;
        gamma = 0.0;
        zheta = 0.0;
        phi = 0.0;
    }


    void update()
    {
    
        if(type == THIRDPERSON || type == THIRDPERSON_MANUAL)
        {
            projectionMatrix = glm::perspective(glm::radians(fov * zoomFactor), float(Viewport::screenWidth) / float(Viewport::screenHeight), 0.1f, 500.0f);
            float a = gamma,b = zheta;
            if(type == THIRDPERSON)
            {
                a = ((Viewport::xpos / Viewport::screenWidth) - 0.5) * M_PI * 3;
                b = ((Viewport::ypos / Viewport::screenHeight) - 0.5) * M_PI * 3;
            }
    
            zoomSpeed += Viewport::scrollY * deltaTime;
            zoomFactor -= zoomSpeed * deltaTime;
            zoomSpeed -= zoomSpeed * deltaTime * zoomDamping;
            
            zoomFactor = std::max(zoomFactor,0.0f);
    
            Viewport::scrollY = 0.0;
    
            viewMatrix = mat4(1.0);
            viewMatrix = translate(viewMatrix,vec3(0,0,-d));
    
            viewMatrix = rotate(viewMatrix,-glm::radians(phi),vec3(0,0,1));
            viewMatrix = rotate(viewMatrix,b,vec3(1,0,0));          //  b
            viewMatrix = rotate(viewMatrix,a,vec3(0,1,0));          // -a
            viewMatrix = translate(viewMatrix,-focusOrigin);
        }
    
        if  (type == ORTHOGONAL)
        {
            projectionMatrix = glm::ortho(l,r,b,t,zmin,zmax);
        }

        invViewMatrix = glm::inverse(viewMatrix);
    
    }

    void flush()
    {
            glUniformMatrix4fv(MaterialLoader::current()[UNIFORM_PROJECTION_MATRIX],1,false,&projectionMatrix[0][0]);
            glUniformMatrix4fv(MaterialLoader::current()[UNIFORM_VIEW_MATRIX],1,false,&viewMatrix[0][0]);
            glUniform3fv(MaterialLoader::current()[UNIFORM_VIEW_POS],1,&invViewMatrix[3][0]);
    }
};

using CameraID = size_t;

namespace CameraLoader
{
    vector<Camera> cameras;

    inline CameraID load(const Camera& camera)
    {
        cameras.push_back(camera);
        return cameras.size() - 1;
    }
};

/** 
 * Mantiene todos los aributos globales de la escena actual
 */
namespace Scene {
    CameraID currentCamera = 0;
    float time = 0.0;

    void update()
    {
        CameraLoader::cameras[currentCamera].update();
    }

    void flush()
    {
        CameraLoader::cameras[currentCamera].flush();
        glUniform1f(       MaterialLoader::current()[UNIFORM_TIME],time);
    }
};

struct MeshBuffer
{
    vector<GLfloat> meshBuffer;

    size_t vertexCount;
    size_t vertexStride;

    size_t normalsPointer;
    size_t tangentsPointer;
    size_t bitangentsPointer;

    bool hasNormals;
    bool hasTangents;
    bool hasBitangents;

    MeshBuffer(const GLfloat* raw_meshBuffer,int _vertexCount,int _vertexStride) : 
    vertexCount(_vertexCount),
    vertexStride(_vertexStride),
    meshBuffer(_vertexCount * _vertexStride)
    {
        memcpy(&meshBuffer[0],raw_meshBuffer,vertexCount * vertexStride * sizeof(GLfloat));
    }

    size_t allocateRegion()
    {
        size_t ptr = meshBuffer.size();
        meshBuffer.resize(meshBuffer.size() + vertexCount * 3);
        return ptr;
    }

    void generateNormals()
    {
        normalsPointer = allocateRegion();

        for (int i = 0,j = 0; i < vertexCount * vertexStride; i += 3 * vertexStride, j += 9)
        {
            const glm::vec3 x = *(glm::vec3 *)&meshBuffer[i];
            const glm::vec3 y = *(glm::vec3 *)&meshBuffer[i + vertexStride];
            const glm::vec3 z = *(glm::vec3 *)&meshBuffer[i + vertexStride*2];

            glm::vec3 normal = -glm::normalize(glm::cross(x - y,x - z));

            *(glm::vec3*)&meshBuffer[normalsPointer + j] = normal;
            *(glm::vec3*)&meshBuffer[normalsPointer + j + 3] = normal;
            *(glm::vec3*)&meshBuffer[normalsPointer + j + 6] = normal;
        }

        hasNormals = true;
    }

    void generateTangents()
    {
        tangentsPointer = allocateRegion();
        bitangentsPointer = allocateRegion();
        for (int i = 0,j = 0; i < vertexCount * vertexStride; i += 3 * vertexStride, j += 9)
        {
            const glm::vec3 v0 = *(glm::vec3 *)&meshBuffer[i];
            const glm::vec3 v1 = *(glm::vec3 *)&meshBuffer[i + vertexStride];
            const glm::vec3 v2 = *(glm::vec3 *)&meshBuffer[i + vertexStride*2];

            const glm::vec2 uv0 = *(glm::vec3 *)&meshBuffer[i + 6];
            const glm::vec2 uv1 = *(glm::vec3 *)&meshBuffer[i + vertexStride + 6];
            const glm::vec2 uv2 = *(glm::vec3 *)&meshBuffer[i + vertexStride*2 + 6];
            
            glm::vec3 deltaPos1 = v1-v0;
            glm::vec3 deltaPos2 = v2-v0;

            // UV delta
            glm::vec2 deltaUV1 = uv1-uv0;
            glm::vec2 deltaUV2 = uv2-uv0;

            float r = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x);
            glm::vec3 tangent = (deltaPos1 * deltaUV2.y   - deltaPos2 * deltaUV1.y)*r;
            glm::vec3 bitangent = (deltaPos2 * deltaUV1.x   - deltaPos1 * deltaUV2.x)*r;

            *(glm::vec3*)&meshBuffer[tangentsPointer + j] = tangent;
            *(glm::vec3*)&meshBuffer[tangentsPointer + j + 3] = tangent;
            *(glm::vec3*)&meshBuffer[tangentsPointer + j + 6] = tangent;


            *(glm::vec3*)&meshBuffer[bitangentsPointer + j] = bitangent;
            *(glm::vec3*)&meshBuffer[bitangentsPointer + j + 3] = bitangent;
            *(glm::vec3*)&meshBuffer[bitangentsPointer + j + 6] = bitangent;
        }

        hasTangents = true;
        hasBitangents = true;

    }
    void print() const
    {
        for (size_t i = 0; i < vertexCount; i++) {
            for (size_t j = 0; j < vertexStride; j++) {
                cout << meshBuffer[i*vertexStride + j] << " ";
            }
            cout << endl;
        }

        if (hasNormals) {
            for (size_t i = normalsPointer; i < meshBuffer.size(); i+= 3) {
                cout << meshBuffer[i] << " " << meshBuffer[i+1] << " " << meshBuffer[i+2] << endl;
            }
        }
    }

    inline void bufferData() const
    {
        glBufferData(GL_ARRAY_BUFFER, meshBuffer.size() * sizeof(GLfloat), (const GLfloat*)&meshBuffer[0], GL_STATIC_DRAW);
    }
    void bindRegions() const
    {
        size_t rowSize = vertexStride * sizeof(GLfloat);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,rowSize,(void*)0);
        glEnableVertexAttribArray(0);
    
        glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,rowSize,(void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
    
        if (vertexStride > 6)
        {
            glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,rowSize,(void*)(6 * sizeof(float)));
            glEnableVertexAttribArray(2);
        }
        
        if (hasNormals)
        {
            glVertexAttribPointer(3,3,GL_FLOAT,GL_FALSE,3 * sizeof(float),(void*)(normalsPointer * sizeof(float)));
            glEnableVertexAttribArray(3);
        }
        if (hasTangents)
        {    
            glVertexAttribPointer(4,3,GL_FLOAT,GL_FALSE,3 * sizeof(float),(void*)(tangentsPointer * sizeof(float)));
            glEnableVertexAttribArray(4);
        }
        if(hasBitangents)
        {    
            glVertexAttribPointer(5,3,GL_FLOAT,GL_FALSE,3 * sizeof(float),(void*)(bitangentsPointer * sizeof(float)));
            glEnableVertexAttribArray(5);
        }
    }
    inline const GLfloat* raw() const
    {
        return (const GLfloat*)&meshBuffer[0];
    }
};
using MeshID = size_t;
struct Mesh {
    GLuint vao,vbo,ebo;
    size_t vertexCount;
    size_t vertexStride;
    shared_ptr<MeshBuffer> meshBuffer;

    Mesh(const GLfloat* raw_meshBuffer,int _vertexCount,int _vertexStride) : vertexCount(_vertexCount), vertexStride(_vertexStride),
    meshBuffer(new MeshBuffer(raw_meshBuffer,_vertexCount,_vertexStride)) { }

    inline const GLfloat* meshPtr() const { return (const GLfloat*)&meshBuffer->meshBuffer[0]; }

};

struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoords;
};
struct PreparedMesh
{


    GLuint VAO,VBO,EBO;
    std::vector<Vertex> vertices;
    std::vector<size_t> indices;

    PreparedMesh(const std::vector<Vertex>& _vertices, const std::vector<size_t> _indices) :
        vertices(_vertices), 
        indices(_indices) { }

    void glGen()
    {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);

        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);  

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), 
                     &indices[0], GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);	
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

        glEnableVertexAttribArray(1);	
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

        glEnableVertexAttribArray(2);	
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));

        glBindVertexArray(0);
    }
};


#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
class PreparedScene
{
    string directory;
    vector<PreparedMesh> meshes;

    void loadScene(const string& path)
    {
        Assimp::Importer import;
        const aiScene *scene = import.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs);	
    
        if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) 
        {
            cerr << "ERROR::ASSIMP::" << import.GetErrorString() << endl;
            throw std::runtime_error("Failed to load scene");
        }
        directory = path.substr(0, path.find_last_of('/'));

        processNode(scene->mRootNode, scene);
    }

    void processNode(aiNode *node,const aiScene *scene)
    {
         // process all the node's meshes (if any)
        for(unsigned int i = 0; i < node->mNumMeshes; i++)
        {
            aiMesh *mesh = scene->mMeshes[node->mMeshes[i]]; 
            meshes.push_back(processMesh(mesh, scene));			
        }
        // then do the same for each of its children
        for(unsigned int i = 0; i < node->mNumChildren; i++)
        {
            processNode(node->mChildren[i], scene);
        }
    }
    PreparedMesh processMesh(aiMesh *mesh,const aiScene *scene)
    {
        
    }

    public:
    PreparedScene(const string& path)
    {
        loadScene(path);
    }
    
};

/**
 * Mantiene un vector de todas las mesh disponibles a usar y algunas funciones utilitarias para crear Meshes
 */
namespace MeshLoader
{
    vector<Mesh> meshes;
    MeshID currentMesh = -1;

    MeshID loadMesh(Mesh&& mesh)
    {
        meshes.emplace_back(mesh);
        return meshes.size() - 1;
    }

    static const GLfloat triangle_mesh[] = {
       -1.0f, -1.0f, 0.0f, 1.0,0.0,0.0,
       1.0f, -1.0f, 0.0f,  0.0,1.0,0.0,
       0.0f,  1.0f, 0.0f,  0.0,0.0,1.0f
    };

    static const GLfloat textured_plain_mesh[] = {
       -1.0f, -1.0f, 0.0f, 1.0,0.0,0.0,  0.0,0.0,
       1.0f, -1.0f, 0.0f,  0.0,1.0,0.0,  1.0,0.0,
       0.0f,  1.0f, 0.0f,  0.0,0.0,1.0f, 0.5,0.5,
    };

    static const GLfloat cube_mesh[] = {
    
        // -Z
        -1.0,1.0,-1.0,    0.0,1.0,0.0,  0.0,1.0,
        -1.0,-1.0,-1.0,   1.0,0.0,0.0,  0.0,0.0,
        1.0,1.0,-1.0,     0.0,0.0,1.0,  1.0,1.0,
    
        1.0,-1.0,-1.0,    0.0,1.0,0.0,  1.0,0.0,
        1.0,1.0,-1.0,     0.0,0.0,1.0,  1.0,1.0,
        -1.0,-1.0,-1.0,   1.0,0.0,0.0,  0.0,0.0,
    
        // +Z
    
        -1.0,-1.0,1.0,    1.0,0.0,0.0,  1.0,1.0,
        -1.0,1.0,1.0,     0.0,1.0,0.0,  1.0,0.0,
        1.0,1.0,1.0,      0.0,0.0,1.0,  0.0,0.0,
                                                
        1.0,1.0,1.0,      0.0,0.0,1.0,  0.0,0.0,
        1.0,-1.0,1.0,     0.0,1.0,0.0,  0.0,1.0,
        -1.0,-1.0,1.0,    1.0,0.0,0.0,  1.0,1.0,
    
        // +Y
    
        -1.0,1.0,-1.0,    0.0,1.0,0.0,  0.0,1.0,
        1.0,1.0,-1.0,     0.0,0.0,1.0,  0.0,0.0,
        -1.0,1.0,1.0,     0.0,1.0,0.0,  1.0,1.0,
                                                
                                        
        -1.0,1.0,1.0,     0.0,1.0,0.0,  1.0,1.0,
        1.0,1.0,-1.0,     0.0,0.0,1.0,  0.0,0.0,
        1.0,1.0,1.0,      0.0,0.0,1.0,  1.0,0.0,        
                                                
                                                
        // -Y                           
                                        
        1.0,-1.0,-1.0,    0.0,1.0,0.0,  1.0,1.0,
        -1.0,-1.0,-1.0,   1.0,0.0,0.0,  1.0,0.0,
        -1.0,-1.0,1.0,    1.0,0.0,0.0,  0.0,0.0,
                                                
                                        
        1.0,-1.0,-1.0,    0.0,1.0,0.0,  1.0,1.0,
        -1.0,-1.0,1.0,    1.0,0.0,0.0,  0.0,0.0,
        1.0,-1.0,1.0,     0.0,1.0,0.0,  0.0,1.0,
    
        // +X
    
        1.0,1.0,-1.0,     0.0,0.0,1.0,   0.0,0.0,
        1.0,-1.0,-1.0,    0.0,1.0,0.0,   0.0,1.0,
        1.0,-1.0,1.0,     0.0,1.0,0.0,   1.0,1.0,
                                                 
        1.0,-1.0,1.0,     0.0,1.0,0.0,   1.0,1.0,
        1.0,1.0,1.0,      0.0,0.0,1.0,   1.0,0.0,
        1.0,1.0,-1.0,     0.0,0.0,1.0,   0.0,0.0,
                                                 
        // -X                                    
                                                 
        -1.0,-1.0,-1.0,    1.0,0.0,0.0,  0.0,1.0,
        -1.0,1.0,-1.0,     0.0,1.0,0.0,  0.0,0.0,
        -1.0,-1.0,1.0,     1.0,0.0,0.0,  1.0,1.0,
                                                 
        -1.0,1.0,1.0,      0.0,1.0,0.0,  1.0,0.0,
        -1.0,-1.0,1.0,     1.0,0.0,0.0,  1.0,1.0,
        -1.0,1.0,-1.0,     0.0,1.0,0.0,  0.0,0.0
    
    };
    
    
    enum PrimitiveMeshType
    {
        Triangle,Plain,Cube
    };
    Mesh createPrimitiveMesh(PrimitiveMeshType type,bool uv = false)
    {
        GLuint VAO,VBO;
        glGenVertexArrays(1, &VAO);
        glBindVertexArray(VAO);
    
        glGenBuffers(1, &VBO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);

        int vertexCount;
        const GLfloat* meshArrayPtr;
        int vertexStride = uv ? 8 : 6;

        switch(type)
        {
            case Triangle:
            vertexCount = 3;
            meshArrayPtr = triangle_mesh;
            break;

            case Plain:
            vertexCount = 3;
            meshArrayPtr = textured_plain_mesh;
            break;

            case Cube:
            vertexCount = 36;
            meshArrayPtr = cube_mesh;
        }

        Mesh mesh(meshArrayPtr,vertexCount,vertexStride);
        mesh.meshBuffer->generateNormals();
        mesh.meshBuffer->generateTangents();
        mesh.meshBuffer->bufferData();
        mesh.meshBuffer->bindRegions();
        return mesh;
    }
};

namespace Renderer
{ 
    extern size_t currentFrame;
    inline void useMaterial(MaterialID id);
    inline void useMaterialInstance(MaterialInstanceID id);
    inline void useMesh(MeshID id);
}
using LightID = size_t;

namespace Light
{
    const static size_t maxLights = 6;
    bool flushUniforms = true;

    vector<glm::vec3> lightsPositions;
    vector<glm::vec3> lightsColor;

    inline LightID load(glm::vec3 pos,glm::vec3 color)
    {
        lightsPositions.push_back(pos);
        lightsColor.push_back(color);
        flushUniforms = true;
        return lightsPositions.size() - 1;
    }

    inline void flush()
    {
        Material& mat = MaterialLoader::materials[MaterialLoader::currentMaterial];
        if (flushUniforms && mat.isLightSensitive())
        {
            glUniform3fv(mat.uniforms[UNIFORM_LIGHTPOSITION],lightsPositions.size(),(GLfloat*)&lightsPositions[0]);
            glUniform3fv(mat.uniforms[UNIFORM_LIGHTCOLOR],lightsColor.size(),(GLfloat*)&lightsColor[0]);
            glUniform1i(mat.uniforms[UNIFORM_LIGHTCOUNT],lightsColor.size());
            
            REGISTER_LIGHT_FLUSH();
        }
    }
};


using ModelID = size_t;
struct Model
{
    MeshID meshID;
    MaterialID materialID;
    MaterialInstanceID materialInstanceID = -1;

    mat4 transformMatrix;
    mat3 normalMatrix;

    Model(MeshID _meshID,MaterialID _materialID = 0) : meshID(_meshID), materialID(_materialID), transformMatrix(1.0f) { }

    inline void draw()
    {
        Renderer::useMaterial(materialID);
        if (materialInstanceID != -1) Renderer::useMaterialInstance(materialInstanceID);
        
        const Mesh& mesh = MeshLoader::meshes[meshID];
        Renderer::useMesh(meshID);
        
        normalMatrix = glm::transpose(glm::inverse(transformMatrix));
        
        glUniformMatrix4fv(MaterialLoader::current()[UNIFORM_TRANSFORM_MATRIX],1,false,&transformMatrix[0][0]);
        glUniformMatrix3fv(MaterialLoader::current()[UNIFORM_NORMAL_MATRIX],1,false,&normalMatrix[0][0]);
        glDrawArrays(GL_TRIANGLES,0,mesh.vertexCount);
    }

    void process()  {
        
        if (materialID != 3)
        {
            transformMatrix = glm::rotate(transformMatrix,0.1f * deltaTime, vec3(0.2,1,0));
        }
    }

    bool operator<(const Model& model) const
    {
        return materialID < model.materialID || 
            (materialID == model.materialID && (materialInstanceID < model.materialInstanceID || 
                (model.materialInstanceID == model.materialInstanceID && meshID < model.meshID)));
    }
};

namespace ModelLoader
{
    sorted_vector<Model> models;

    ModelID loadModel(const Model& model)
    {
        return models.push_back(model);
    }

    inline Model& get(ModelID modelID) { return models[modelID]; }
};
namespace Util
{
    struct VRP
    {
        vec4 min,max;

        inline vec4 center() const { return vec4(vec3(min + max) * 0.5f,1); }
    };

    VRP getSceneVRP()
    {
        auto models = ModelLoader::models.native();
        vec4 min = vec4(vec3(INFINITY),0);
        vec4 max = vec4(vec3(-INFINITY),0);

        for (size_t i = 0; i <models.size(); i++)
        {
            Mesh& mesh = MeshLoader::meshes[models[i].meshID];
            for (size_t j = 0; j < mesh.vertexCount; j++)
            {
                size_t b = mesh.vertexStride * j;
                vec4 v = mat4(models[i].transformMatrix) * vec4(mesh.meshPtr()[b],mesh.meshPtr()[b + 1],mesh.meshPtr()[b + 2],1.0);
                
                min.x = glm::min(v.x,min.x);
                min.y = glm::min(v.y,min.y);
                min.z = glm::min(v.z,min.z);

                max.x = glm::max(v.x,max.x);
                max.y = glm::max(v.y,max.y);
                max.z = glm::max(v.z,max.z);
            }
        }
        return {min,max};
    }
}
namespace Renderer
{
    size_t currentFrame = 1;

    inline void useMaterial(MaterialID materialID)
    {
        GLuint programID = MaterialLoader::materials[materialID].programID;
        if(MaterialLoader::currentMaterial != materialID)
        {
            MaterialLoader::currentMaterial = materialID;
            MaterialLoader::materials[MaterialLoader::currentMaterial].bind();
            REGISTER_MATERIAL_SWAP();
        }

        if (MaterialLoader::usedMaterials[MaterialLoader::currentMaterial] != currentFrame)
        {
            MaterialLoader::usedMaterials[MaterialLoader::currentMaterial] = currentFrame;
            Scene::flush();
            Light::flush();
        }
    }

    inline void useMesh(MeshID meshID)
    {
        if (meshID != MeshLoader::currentMesh)
        {
            MeshLoader::currentMesh = meshID;
            glBindVertexArray(MeshLoader::meshes[MeshLoader::currentMesh].vao);
            REGISTER_MESH_SWAP();
        }
    }

    inline void useMaterialInstance(MaterialInstanceID instanceID)
    {
        MaterialLoader::materials[MaterialLoader::currentMaterial].useInstance(instanceID);
    }
    namespace Ui
    {
        inline void setup_ui(Window* window)
        {
            const static char* glsl_version = "#version 330";

            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO(); (void)io;

            //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
            //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

            ImGui::StyleColorsDark();

            ImGui_ImplGlfw_InitForOpenGL(window, true);
            ImGui_ImplOpenGL3_Init(glsl_version);
        }
        inline void render_ui()
        { 
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            bool show_demo_window = true;            

            {
                static float f = 0.0f;
                static int counter = 0;
                
                ImGui::Begin("Ortographic camera");                          // Create a window called "Hello, world!" and append into it.
                
                Camera& current = CameraLoader::cameras[Scene::currentCamera];
                ImGui::SliderFloat("l", &current.l, -3.0, 3.0f);
                ImGui::SliderFloat("r", &current.r, -3.0, 3.0f);
                ImGui::SliderFloat("b", &current.b, -3.0, 3.0f);
                ImGui::SliderFloat("t", &current.t, -3.0, 3.0f);
                ImGui::SliderFloat("zmin", &current.zmin, -3.0, 3.0f);
                ImGui::SliderFloat("zmax", &current.zmax, -3.0, 3.0f);
                
                ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
                ImGui::End();

                ImGui::Begin("Perspective camera");                          // Create a window called "Hello, world!" and append into it.
                ImGui::SliderFloat("phi", &current.phi, 0.0, 360.0f);
                ImGui::SliderFloat("zheta", &current.zheta, 0.0, 360.0f);
                ImGui::SliderFloat("gamma", &current.gamma, 0.0, 360.0f);
                ImGui::SliderFloat("d", &current.d, 0.0f, 6.0f);
                ImGui::End();
            }

            ImGui::Render();
        }
    };
    int render_loop(Window* window)
    {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);  
        glEnable(GL_DEPTH_TEST);    
        glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
        
        auto& models = ModelLoader::models.native();
        bool inverseOrder = false;
        do{

            REGISTER_FRAME();
            currentFrame++;

            glClearColor(0.0,0.0,0.0,1.0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            
            Scene::time += deltaTime;
            Scene::update();

            if (inverseOrder)
            {
                for(int i = 0; i < models.size(); i++)
                {
                    models[i].process();
                    models[i].draw();
                }
            }
            else
            {
                for(int i = models.size() - 1; i >= 0; i--)
                {
                    models[i].process();
                    models[i].draw();
                }
            }
            inverseOrder = !inverseOrder;

            Ui::render_ui();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());     //For ui

            glfwSwapBuffers(window);
            glfwPollEvents();

            LOG_FRAME();

            Light::flushUniforms = false;
        
        }
        while( glfwGetKey(window, GLFW_KEY_ESCAPE ) != GLFW_PRESS &&
               glfwWindowShouldClose(window) == 0 );
    
        return 0;
    }
};


//Funciones especificas de testeo
void loadSpecificMaterials()
{
    
    MaterialInstance debugMaterialInstance;
    debugMaterialInstance.setTexture(Texture::loadTexture(TextureData("uvgrid.png")),0);

    MaterialInstance container({Uniform(3.3f)});
    
    container.setTexture(Texture::loadTexture(TextureData("metal_base.jpg")),0);
    container.setTexture(Texture::loadTexture(TextureData("metal_specular.jpg")),1);
    container.setTexture(Texture::loadTexture(TextureData("metal_normal.jpg")),2);
    MaterialInstanceLoader::loadMaterialInstance(container);

    MaterialLoader::loadMaterial(Material("primitive",list<string>()));
    MaterialLoader::loadMaterial(Material("emissive",{"emissive","factor"}));
    Material light("light",{"shinness"});

    MaterialLoader::loadMaterial(light);
    MaterialLoader::loadMaterial(Material("unshaded",{"shadecolor"}));


    Material debug("debug",list<string>());
    Material textured("textured",list<string>());
    MaterialLoader::loadMaterial(textured);

    Material textured2("textured",list<string>());
    MaterialLoader::loadMaterial(textured2);

    
    MaterialLoader::debugMaterialID = MaterialLoader::loadMaterial(debug);
    MaterialLoader::debugMaterialInstanceID = MaterialInstanceLoader::loadMaterialInstance(debugMaterialInstance);
}
void loadSpecificWorld()
{
    //meshes.emplace_back(createPrimitiveMesh(Cube));
    
    /*
     * Marabunta world
     */
    /*
    {
    MeshID cube = MeshLoader::loadMesh(MeshLoader::createPrimitiveMesh(MeshLoader::Cube,true));
    MeshID plain = MeshLoader::loadMesh(MeshLoader::createPrimitiveMesh(MeshLoader::Plain,true));

    vec3 midPoint = vec3(0.0);
    int l = 100;
    for (size_t i = 0; i < l; i++)
    {
        int s = 100;
        vec3 randPos = vec3(rand() % s,rand() % s,rand() % s);
        midPoint += randPos;

        Model model(i % 2);
        model.transformMatrix = glm::translate(mat4(model.transformMatrix),randPos);
        model.materialID = i % 3 + 1;

        ModelLoader::loadModel(model);


    }
    }*/
    // Scene::focusOrigin = midPoint * 1.0f / float(l);

/*
    MeshID plain = MeshLoader::loadMesh(MeshLoader::createPrimitiveMesh(MeshLoader::Plain,true));
    Model plainA(plain);
    plainA.materialID = 1;
    plainA.transformMatrix = scale(plainA.transformMatrix,vec3(3.0,3.0,3.0));
    ModelLoader::loadModel(plainA);
  */  
    
    CameraLoader::load(Camera());

    
    Model cube = Model(MeshLoader::loadMesh(MeshLoader::createPrimitiveMesh(MeshLoader::Cube,true)));
    cube.materialID = 2;
    cube.materialInstanceID = 0;
    ModelLoader::loadModel(cube);
    Model cube2 = cube;

    for (size_t i = 1; i < 4; i++)
    {
        for (size_t j = 1; j < 4; j++)
        {
            cube2.transformMatrix = glm::translate(mat4(cube.transformMatrix),vec3(2.2 * i,2.2 * j,0.0));
            ModelLoader::loadModel(cube2);
        }
    }
    


    ModelLoader::loadModel(cube2);

    Model cube3 = cube2;
    for (size_t i = 0; i < 4; i++)
    {

        vec3 lightPosition(rand() % 10 - 5,rand() % 10 - 5,rand() % 10 - 5);
        cube3.transformMatrix = glm::scale(glm::translate(mat4(1.0),lightPosition),vec3(0.1));
        cube3.materialID = 3;
        vec4 color((rand() % 255) / 255.0f,(rand() % 255) / 255.0f,(rand() % 255 ) / 255.0f,1.0);
        MaterialInstance unshadedColor(1);
        unshadedColor.set(0,color);
        cube3.materialInstanceID = MaterialInstanceLoader::loadMaterialInstance(unshadedColor);
        
        ModelLoader::loadModel(cube3);
        Light::load(cube3.transformMatrix[3],vec3(color));
    }
}

int main(int argc, char** argv)
{
    
    Window *window = createWindow();
    
    glfwSetCursorPosCallback(window, Viewport::cursor_position_callback);
    glfwSetFramebufferSizeCallback(window, Viewport::framebuffer_size_callback);
    glfwSetScrollCallback(window, Viewport::scroll_callback);

    loadSpecificMaterials();
    loadSpecificWorld();
    
    Renderer::Ui::setup_ui(window);
    return Renderer::render_loop(window);
}

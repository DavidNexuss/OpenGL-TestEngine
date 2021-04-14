#define _GLIBCXX_PARALLEL

#include "window.h"
#include "load_shader.h"
#include <iostream>
#include <vector>
#include <list>
#include <map>
#include <glm/glm.hpp>
#include <glm/ext.hpp>


#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

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
    const vector<T>& native() {return values; }
};

namespace Debug
{
    int materialSwaps;
    int meshSwaps;
    int textureSwaps;

    inline void reset()
    {
        materialSwaps = 0;
        meshSwaps = 0;
        textureSwaps = 0;
    }

    inline void print()
    {
        cerr << "Frame debug information: " << endl;
        cerr << "Material swaps :" << materialSwaps << endl;
        cerr << "Mesh swaps :" << meshSwaps << endl;
        cerr << "Texture swaps :" << textureSwaps << endl;
        cerr << "----" << endl;
    }
}

#define DEBUG
#ifdef DEBUG 
    #define REGISTER_FRAME() Debug::reset()
    #define REGISTER_MATERIAL_SWAP() Debug::materialSwaps++
    #define REGISTER_MESH_SWAP() Debug::meshSwaps++
    #define REGISTER_TEXTURE_SWAP() Debug::textureSwaps++;
    #define LOG_FRAME() Debug::print()
#else
    #define REGISTER_FRAME()
    #define REGISTER_MATERIAL_SWAP()
    #define REGISTER_MESH_SWAP()
    #define REGISTER_TEXTURE_SWAP()
    #define LOG_FRAME()
#endif

/**
 * Mantiene todos los atributos del viewport y de glfw a procesar,incluido el input
 */
namespace Viewport
{
    double screenWidth,screenHeight;
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

namespace Directory
{
    const string texturePrefix = "textures/";
    const string materialPrefix = "materials/";
}

struct TextureData
{
    int width, height, nrChannels;
    unsigned char* data;

    TextureData(const string& path)
    {
        data = stbi_load((Directory::texturePrefix + path).c_str(),&width,&height,&nrChannels,0);
        if (! data)
        {
            cerr << "Error loading texture " << path << " from disk" << endl;
        }
    }
};
using TextureID = size_t;
namespace Texture
{
    
    vector<TextureData> texturesData;
    vector<GLuint> glTexturesIds;
    vector<TextureID> texturesUnits(16);

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

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, textureData.width, textureData.height, 0, GL_RGB, GL_UNSIGNED_BYTE, textureData.data);
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

enum UniformBasics
{
    PROJECTION_MATRIX = 0,
    VIEW_MATRIX,
    TRANSFORM_MATRIX,
    TIME
};

using MaterialID = size_t;

struct Material
{
    GLuint programID;
    vector<GLuint> uniforms;
    struct TextureAssingment
    {
        TextureID textureID;
        int textureUnit;
        GLuint uniformSlot;
    };

    vector<TextureAssingment> assignedTextures;

    /*
     * pre:
     * L'ordre dels uniforms, si n'hi han es el següent:
     *  * mat4 projectionMatrix
     *  * mat4 viewMatrix
     *  * mat4 transformMatrix
     *  * float time
     */
    
    void loadShaderUniforms(const list<string>& uniformsList)
    { 
        uniforms.emplace_back(glGetUniformLocation(programID,"projectionMatrix"));
        uniforms.emplace_back(glGetUniformLocation(programID,"viewMatrix"));
        uniforms.emplace_back(glGetUniformLocation(programID,"transformMatrix"));
        uniforms.emplace_back(glGetUniformLocation(programID,"time"));
    
        for(const auto& uniform : uniformsList)
        {
            uniforms.emplace_back(glGetUniformLocation(programID,uniform.c_str()));
            //TODO: check uniform not found
        }
    }
    Material(string materialName,const list<string>& uniforms)
    {
        string fragmentPath = Directory::materialPrefix + materialName + "_fragment.glsl";
        string vertexPath = Directory::materialPrefix + materialName + "_vertex.glsl";
    
        programID = compileShader(vertexPath.c_str(),fragmentPath.c_str());
        
        if (programID < 0)
        {
            cerr << "Error loading shaders!!!" << endl;
            throw std::runtime_error("Error compiling shader");
        }

        loadShaderUniforms(uniforms);
    }

    bool addTexture(TextureID textureID,int textureUnit)
    {
        string uniformLocation = "texture" + assignedTextures.size();
        GLuint uniformSlot = glGetUniformLocation(programID,uniformLocation.c_str());
        if (uniformSlot < 0) return false;
        assignedTextures.push_back({textureID,textureUnit,uniformSlot});
        return true;
    }

    inline void bind()
    {
        glUseProgram(programID);
        for (int i = 0; i < assignedTextures.size(); i++)
        {
            Texture::useTexture(assignedTextures[i].textureID,assignedTextures[i].textureUnit);
        }
    }
};

/** 
 * Mantiene todos los shaders cargados i crea la abstracción de Material
 */
namespace MaterialLoader
{

    vector<Material> materials;
    vector<size_t> usedMaterials;   //Usado para saber si el material ha sido usado en el frame actual


    /*
     * Garanteix una referencia a un shader ja carregat en el sistema
     */
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

/** 
 * Mantiene todos los aributos globales de la escena actual
 */
namespace Scene {
    mat4 projectionMatrix, viewMatrix;
    const float fov = 80.0;
    float time = 0.0;

    float zoomFactor = 1.0;
    float zoomSpeed = 0.0;
    const float zoomDamping = 0.6;

    vec3 focusOrigin;

    void update()
    {
        projectionMatrix = glm::perspective(glm::radians(fov * zoomFactor), 800.0f / 600.0f, 0.1f, 500.0f);
        
        //Camera en 3ra persona amb una rotació completa x3
        float a = ((Viewport::xpos / Viewport::screenWidth) - 0.5) * M_PI * 3;
        float b = ((Viewport::ypos / Viewport::screenHeight) - 0.5) * M_PI * 3;


        zoomSpeed += Viewport::scrollY * deltaTime;
        zoomFactor -= zoomSpeed * deltaTime;
        zoomSpeed -= zoomSpeed * deltaTime * zoomDamping;
        
        zoomFactor = std::max(zoomFactor,0.0f);

        Viewport::scrollY = 0.0;

        viewMatrix = mat4(1.0);
        viewMatrix = translate(viewMatrix,vec3(0,0,-5.0));

     // viewMatrix = rotate(viewMatrix,roll,vec3(0,0,1))
        viewMatrix = rotate(viewMatrix,b,vec3(1,0,0));          //  b
        viewMatrix = rotate(viewMatrix,a,vec3(0,1,0));          // -a
        
        //View reference point
        viewMatrix = translate(viewMatrix,-focusOrigin);

    }

    void flush()
    {

        glUniformMatrix4fv(MaterialLoader::current()[PROJECTION_MATRIX],1,false,&projectionMatrix[0][0]);
        glUniformMatrix4fv(MaterialLoader::current()[VIEW_MATRIX],1,false,&viewMatrix[0][0]);
        glUniform1f(       MaterialLoader::current()[TIME],time);
    }
};

/**
 * Representa una Mesh(identificada por un únivo VAO) ya cargada en memoria
 */
struct Mesh {
    GLuint vao,vbo;
    int vertexCount;
};

using MeshID = size_t;

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
    
        -1.0,-1.0,1.0,    1.0,0.0,0.0,  0.0,1.0,
        -1.0,1.0,1.0,     0.0,1.0,0.0,  0.0,0.0,
        1.0,1.0,1.0,      0.0,0.0,1.0,  1.0,1.0,
                                                
        1.0,1.0,1.0,      0.0,0.0,1.0,  1.0,0.0,
        1.0,-1.0,1.0,     0.0,1.0,0.0,  1.0,1.0,
        -1.0,-1.0,1.0,    1.0,0.0,0.0,  0.0,0.0,
    
        // +Y
    
        -1.0,1.0,-1.0,    0.0,1.0,0.0,  0.0,1.0,
        1.0,1.0,-1.0,     0.0,0.0,1.0,  0.0,0.0,
        -1.0,1.0,1.0,     0.0,1.0,0.0,  1.0,1.0,
                                                
                                        
        -1.0,1.0,1.0,     0.0,1.0,0.0,  1.0,0.0,
        1.0,1.0,-1.0,     0.0,0.0,1.0,  1.0,1.0,
        1.0,1.0,1.0,      0.0,0.0,1.0,  0.0,0.0,        
                                                
                                                
        // -Y                           
                                        
        1.0,-1.0,-1.0,    0.0,1.0,0.0,  0.0,1.0,
        -1.0,-1.0,-1.0,   1.0,0.0,0.0,  0.0,0.0,
        -1.0,-1.0,1.0,    1.0,0.0,0.0,  1.0,1.0,
                                                
                                        
        1.0,-1.0,-1.0,    0.0,1.0,0.0,  1.0,0.0,
        -1.0,-1.0,1.0,    1.0,0.0,0.0,  1.0,1.0,
        1.0,-1.0,1.0,     0.0,1.0,0.0,  0.0,0.0,
    
        // +X
    
        1.0,1.0,-1.0,     0.0,0.0,1.0,   0.0,1.0,
        1.0,-1.0,-1.0,    0.0,1.0,0.0,   0.0,0.0,
        1.0,-1.0,1.0,     0.0,1.0,0.0,   1.0,1.0,
                                                 
        1.0,-1.0,1.0,     0.0,1.0,0.0,   1.0,0.0,
        1.0,1.0,1.0,      0.0,0.0,1.0,   1.0,1.0,
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
        Mesh mesh;
        glGenVertexArrays(1, &mesh.vao);
        glBindVertexArray(mesh.vao);
    
        glGenBuffers(1, &mesh.vbo);
        glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    
        switch(type)
        {
            case Triangle:
            glBufferData(GL_ARRAY_BUFFER, sizeof(triangle_mesh), triangle_mesh, GL_STATIC_DRAW);
            mesh.vertexCount = 3;
            break;

            case Plain:
            glBufferData(GL_ARRAY_BUFFER, sizeof(textured_plain_mesh), textured_plain_mesh, GL_STATIC_DRAW);
            mesh.vertexCount = 3;
            break;

            case Cube:
            glBufferData(GL_ARRAY_BUFFER, sizeof(cube_mesh), cube_mesh, GL_STATIC_DRAW);
            mesh.vertexCount = 36;
        }
    
        int rowSize = uv ? 8 : 6;
        rowSize *= sizeof(float);

        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,rowSize,(void*)0);
        glEnableVertexAttribArray(0);
    
        glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,rowSize,(void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
    
        if (uv)
        {
            glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,rowSize,(void*)(6 * sizeof(float)));
            glEnableVertexAttribArray(2);
        }
        return mesh;
    }
};

namespace Renderer
{ 
    inline void useMaterial(MaterialID id); 
    inline void useMesh(MeshID id);
}
/**
 * Representa un modelo dentro de la escena 3D
 * Representado por una mesh una matriz de transformación y un material
 */

struct Model
{
    MeshID meshID;
    MaterialID materialID;
    mat4 transformMatrix = mat4(1.0);

    Model(MeshID _meshID,MaterialID _materialID = 0) : meshID(_meshID), materialID(_materialID) { }

    inline void draw()
    {
        const Mesh& mesh = MeshLoader::meshes[meshID];

        Renderer::useMaterial(materialID);
        Renderer::useMesh(meshID);

        glUniformMatrix4fv(MaterialLoader::current()[TRANSFORM_MATRIX],1,false,&transformMatrix[0][0]);
        glDrawArrays(GL_TRIANGLES,0,mesh.vertexCount);
    }
    virtual void process() 
    {

    }

    bool operator<(const Model& model)
    {
        // return meshID < model.meshID || (meshID == model.meshID && (materialID < model.materialID ));
        return materialID < model.materialID || (materialID == model.materialID && meshID < model.meshID);
    }
};

using ModelID = size_t;
/**
 * Mantiene un vector de todos los Modelos cargados y disponible para ser usados
 */

namespace ModelLoader
{
    sorted_vector<Model> models;

    ModelID loadModel(const Model& model)
    {
        return models.push_back(model);
    }

    inline Model& get(ModelID modelID) { return models[modelID]; }
};

/**
 * Conjunto de funciones para renderizar la escena
 */
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

        if (MaterialLoader::usedMaterials[materialID] != currentFrame)
        {
            MaterialLoader::usedMaterials[materialID] = currentFrame;
            Scene::flush();
        }
    }

    inline void useMesh(MeshID meshID)
    {
        if (meshID != MeshLoader::currentMesh)
        {
            MeshLoader::currentMesh = meshID;
            glBindVertexArray(MeshLoader::meshes[meshID].vao);
            REGISTER_MESH_SWAP();
        }
    }


    int render_loop(Window* window)
    {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);  
        glEnable(GL_DEPTH_TEST);    
        glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
        
        auto models = ModelLoader::models.native();
        do{
            REGISTER_FRAME();

            currentFrame++;
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glClearColor(0.0,0.0,0.0,1.0);
    
            Scene::time += deltaTime;
            Scene::update();
    
            for(int i = 0; i < models.size(); i++)
            {
                models[i].process();
                models[i].draw();
            }
    
            glfwSwapBuffers(window);
            glfwPollEvents();

            LOG_FRAME();
        
        }
        while( glfwGetKey(window, GLFW_KEY_ESCAPE ) != GLFW_PRESS &&
               glfwWindowShouldClose(window) == 0 );
    
        return 0;
    }
};


//Funciones especificas de testeo
void loadSpecificMaterials()
{
    
    MaterialLoader::loadMaterial(Material("primitive",list<string>()));
    MaterialLoader::loadMaterial(Material("emissive",{"emissive"}));

    Material textured("textured",list<string>());
    textured.addTexture(Texture::loadTexture(TextureData("uvgrid.png")),0);
    MaterialLoader::loadMaterial(textured);

    Material textured2("textured",list<string>());
    textured2.addTexture(Texture::loadTexture(TextureData("grass.jpg")),0);
    MaterialLoader::loadMaterial(textured2);

}
void loadSpecificWorld()
{
    //meshes.emplace_back(createPrimitiveMesh(Cube));
    
    /*
     * Marabunta world
     */
    MeshID cube = MeshLoader::loadMesh(MeshLoader::createPrimitiveMesh(MeshLoader::Cube,true));
    MeshID plain = MeshLoader::loadMesh(MeshLoader::createPrimitiveMesh(MeshLoader::Plain,true));

    vec3 midPoint = vec3(0.0);
    int l = 500;
    for (size_t i = 0; i < l; i++)
    {
        int s = 100;
        vec3 randPos = vec3(rand() % s,rand() % s,rand() % s);
        midPoint += randPos;

        Model model(i % 2);
        model.transformMatrix = translate(model.transformMatrix,randPos);
        model.materialID = i % 3 + 1;
        
        ModelLoader::loadModel(model);


    }

    Scene::focusOrigin = midPoint * 1.0f / float(l);

    /*
    MeshID plain = MeshLoader::loadMesh(MeshLoader::createPrimitiveMesh(MeshLoader::Plain,true));
    Model plainA(plain);
    plainA.materialID = 2;
    plainA.transformMatrix = scale(plainA.transformMatrix,vec3(3.0,3.0,3.0));
    ModelLoader::loadModel(plainA);
*/
        /*
    ModelID cube = ModelLoader::loadModel(Model(MeshLoader::loadMesh(MeshLoader::createPrimitiveMesh(MeshLoader::Cube,true))));
    ModelLoader::get(cube).materialID = 2; */
}

int main(int argc, char** argv)
{
    Window *window = createWindow();
    
    glfwSetCursorPosCallback(window, Viewport::cursor_position_callback);
    glfwSetFramebufferSizeCallback(window, Viewport::framebuffer_size_callback);
    glfwSetScrollCallback(window, Viewport::scroll_callback);

    //Estas funciones cargan un mundo específico usnando toda la API
    loadSpecificMaterials();
    loadSpecificWorld();
    
    return Renderer::render_loop(window);
}

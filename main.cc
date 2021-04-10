#include "window.h"
#include "load_shader.h"
#include <iostream>
#include <vector>
#include <list>
#include <map>
#include <glm/glm.hpp>
#include <glm/ext.hpp>

using namespace std;
using namespace glm;

const float deltaTime = 0.1;

/**
 * Mantiene todos los atributos del viewport y de glfw a procesar,incluido el input
 */
namespace Viewport
{
    double screenWidth,screenHeight;
    double xpos,ypos;

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

}

using MaterialID = size_t;

#define PROJECTION_MATRIX 0
#define VIEW_MATRIX 1
#define TRANSFORM_MATRIX 2
#define TIME 3

/** 
 * Mantiene todos los shaders cargados i crea la abstracción de Material
 */
namespace Material
{

    vector<GLuint> shaders;
    vector<size_t> usedMaterials;
    map<MaterialID,vector<GLuint>> params;
    
    /*
     * Garanteix una referencia a un shader ja carregat en el sistema
     */
    MaterialID currentMaterial = -1;
    
    
    //Shader loading functions
    
    /*
     * pre:
     * L'ordre dels uniforms, si n'hi han es el següent:
     *  * mat4 projectionMatrix
     *  * mat4 viewMatrix
     *  * mat4 transformMatrix
     *  * float time
     */
    void loadShaderUniforms(const list<string>& uniforms,MaterialID mat)
    { 
        GLuint programID = shaders[mat];
        params[mat].emplace_back(glGetUniformLocation(programID,"projectionMatrix"));
        params[mat].emplace_back(glGetUniformLocation(programID,"viewMatrix"));
        params[mat].emplace_back(glGetUniformLocation(programID,"transformMatrix"));
        params[mat].emplace_back(glGetUniformLocation(programID,"time"));
    
        for(const auto& uniform : uniforms)
        {
            params[mat].emplace_back(glGetUniformLocation(programID,uniform.c_str()));
            //TODO: check uniform not found
        }
    }
    
    MaterialID loadMaterial(string materialName,const list<string>& uniforms)
    {
        string fragmentPath = "materials/" + materialName + "_fragment.glsl";
        string vertexPath = "materials/" + materialName + "_vertex.glsl";
    
        GLuint programID = compileShader(vertexPath.c_str(),fragmentPath.c_str());
        
        if (programID < 0)
        {
            cerr << "Error loading shaders!!!" << endl;
            return -1;
        }
    
        shaders.push_back(programID);
        usedMaterials.push_back(0);
        MaterialID mat = shaders.size() - 1;
        loadShaderUniforms(uniforms,mat);
        return mat;
    }

    const inline vector<GLuint>& current()
    {
        return params[currentMaterial];
    }
};

/** 
 * Mantiene todos los aributos globales de la escena actual
 */
namespace Scene {
    mat4 projectionMatrix, viewMatrix;
    const float fov = 80.0;
    float time = 0.0;
    vec3 focusOrigin;

    void update()
    {
        projectionMatrix = glm::perspective(glm::radians(fov), 800.0f / 600.0f, 0.1f, 500.0f);
        
        //Camera en 3ra persona amb una rotació completa x3
        float a = ((Viewport::xpos / Viewport::screenWidth) - 0.5) * M_PI * 3;
        float b = ((Viewport::ypos / Viewport::screenHeight) - 0.5) * M_PI * 3;

        viewMatrix = mat4(1.0);

        viewMatrix = translate(viewMatrix,vec3(0,0,-5));
        viewMatrix = rotate(viewMatrix,b,vec3(1,0,0));
        viewMatrix = rotate(viewMatrix,a,vec3(0,1,0));
        viewMatrix = translate(viewMatrix,-focusOrigin);

    }

    void flush()
    {
        if(Material::currentMaterial == -1) return;

        glUniformMatrix4fv(Material::current()[PROJECTION_MATRIX],1,false,&projectionMatrix[0][0]);
        glUniformMatrix4fv(Material::current()[VIEW_MATRIX],1,false,&viewMatrix[0][0]);
        glUniform1f(       Material::current()[TIME],time);
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
    static const GLfloat cube_mesh[] = {
    
        // -Z
        -1.0,1.0,-1.0,    0.0,1.0,0.0,
        -1.0,-1.0,-1.0,   1.0,0.0,0.0,
        1.0,1.0,-1.0,     0.0,0.0,1.0,
    
        1.0,-1.0,-1.0,    0.0,1.0,0.0,
        1.0,1.0,-1.0,     0.0,0.0,1.0,
        -1.0,-1.0,-1.0,   1.0,0.0,0.0,
    
        // +Z
    
        -1.0,-1.0,1.0,   1.0,0.0,0.0,
        -1.0,1.0,1.0,    0.0,1.0,0.0,
        1.0,1.0,1.0,     0.0,0.0,1.0,
    
        1.0,1.0,1.0,     0.0,0.0,1.0,
        1.0,-1.0,1.0,    0.0,1.0,0.0,
        -1.0,-1.0,1.0,   1.0,0.0,0.0,
    
        // +Y
    
        -1.0,1.0,-1.0,   0.0,1.0,0.0,
        1.0,1.0,-1.0,    0.0,0.0,1.0,
        -1.0,1.0,1.0,    0.0,1.0,0.0,
    
    
        -1.0,1.0,1.0,    0.0,1.0,0.0,
        1.0,1.0,-1.0,    0.0,0.0,1.0,
        1.0,1.0,1.0,     0.0,0.0,1.0,
    
    
        // -Y
    
        1.0,-1.0,-1.0,    0.0,1.0,0.0,
        -1.0,-1.0,-1.0,   1.0,0.0,0.0,
        -1.0,-1.0,1.0,    1.0,0.0,0.0,
    
    
        1.0,-1.0,-1.0,    0.0,1.0,0.0,
        -1.0,-1.0,1.0,    1.0,0.0,0.0,
        1.0,-1.0,1.0,     0.0,1.0,0.0,
    
        // +X
    
        1.0,1.0,-1.0,     0.0,0.0,1.0,
        1.0,-1.0,-1.0,    0.0,1.0,0.0,
        1.0,-1.0,1.0,     0.0,1.0,0.0,
    
        1.0,-1.0,1.0,     0.0,1.0,0.0,
        1.0,1.0,1.0,      0.0,0.0,1.0,
        1.0,1.0,-1.0,     0.0,0.0,1.0,
    
        // -X
    
        -1.0,-1.0,-1.0,    1.0,0.0,0.0,
        -1.0,1.0,-1.0,     0.0,1.0,0.0,
        -1.0,-1.0,1.0,     1.0,0.0,0.0,
    
        -1.0,1.0,1.0,      0.0,1.0,0.0,
        -1.0,-1.0,1.0,     1.0,0.0,0.0,
        -1.0,1.0,-1.0,     0.0,1.0,0.0,
    
    };
    
    
    enum PrimitiveMeshType
    {
        Triangle,Cube
    };
    Mesh createPrimitiveMesh(PrimitiveMeshType type)
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
            
            case Cube:
            glBufferData(GL_ARRAY_BUFFER, sizeof(cube_mesh), cube_mesh, GL_STATIC_DRAW);
            mesh.vertexCount = 36;
        }
    
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(float) * 6,(void*)0);
        glEnableVertexAttribArray(0);
    
        glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(float) * 6,(void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
    
        return mesh;
    }

};

namespace Renderer{ inline void useMaterial(MaterialID id); }
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
        glUniformMatrix4fv(Material::current()[TRANSFORM_MATRIX],1,false,&transformMatrix[0][0]);
        glBindVertexArray(mesh.vao);
        glDrawArrays(GL_TRIANGLES,0,mesh.vertexCount);
    }
    virtual void process() 
    {

    }
};

using ModelID = size_t;
/**
 * Mantiene un vector de todos los Modelos cargados y disponible para ser usados
 */

namespace ModelLoader
{
    vector<Model> models;

    ModelID loadModel(const Model& model)
    {
        models.push_back(model);
        return models.size() - 1;
    }
};

/**
 * Conjunto de funciones para renderizar la escena
 */
namespace Renderer
{
    size_t currentFrame = 1;

    inline void useMaterial(MaterialID materialID)
    {
        GLuint programID = Material::shaders[materialID];
        if(Material::currentMaterial != materialID)
        {
            Material::currentMaterial = materialID;
            glUseProgram(programID);
        }

        if (Material::usedMaterials[materialID] != currentFrame)
        {
            Material::usedMaterials[materialID] = currentFrame;
            Scene::flush();
        }
    }


    int render_loop(Window* window)
    {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);  
        glEnable(GL_DEPTH_TEST);    
        glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
        
        do{
            currentFrame++;
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glClearColor(0.0,0.0,0.0,1.0);
    
            Scene::time += deltaTime;
            Scene::update();
            Scene::flush();
    
            for(int i = 0; i < ModelLoader::models.size(); i++)
            {
                ModelLoader::models[i].process();
                ModelLoader::models[i].draw();
            }
    
            glfwSwapBuffers(window);
            glfwPollEvents();
        
        }
        while( glfwGetKey(window, GLFW_KEY_ESCAPE ) != GLFW_PRESS &&
               glfwWindowShouldClose(window) == 0 );
    
        return 0;
    }
};


//Funciones especificas de testeo
void loadSpecificMaterials()
{
    Material::loadMaterial("primitive",list<string>());
    Material::loadMaterial("emissive",{"emissive"});
}
void loadSpecificWorld()
{
    //meshes.emplace_back(createPrimitiveMesh(Cube));
    
    /*
     * Marabunta world
     */
    MeshID cube = MeshLoader::loadMesh(MeshLoader::createPrimitiveMesh(MeshLoader::Cube));
    vec3 midPoint = vec3(0.0);
    int l = 3000;
    for (size_t i = 0; i < l; i++)
    {
        int s = 100;
        vec3 randPos = vec3(rand() % s,rand() % s,rand() % s);
        midPoint += randPos;

        Model model(0);
        model.transformMatrix = translate(model.transformMatrix,randPos);
        if (i % 2) model.materialID = 1;
        
        ModelLoader::loadModel(model);


    }

    Scene::focusOrigin = midPoint * 1.0f / float(l);

    //models.emplace_back(Model(loadMesh(createPrimitiveMesh(Cube))));
    
}

int main(int argc, char** argv)
{
    Window *window = createWindow();
    
    glfwSetCursorPosCallback(window, Viewport::cursor_position_callback);
    glfwSetFramebufferSizeCallback(window, Viewport::framebuffer_size_callback);

    //Estas funciones cargan un mundo específico usnando toda la API
    loadSpecificMaterials();
    loadSpecificWorld();
    
    return Renderer::render_loop(window);
}

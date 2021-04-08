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

#define PROJECTION_MATRIX 0
#define VIEW_MATRIX 1
#define TRANSFORM_MATRIX 2
#define TIME 3

vector<GLuint> shaders;
map<GLuint,vector<GLuint>> params;

GLuint currentShader = -1;

void loadShaderUniforms(const list<string>& uniforms,GLuint shader)
{ 
    params[shader].emplace_back(glGetUniformLocation(shader,"projectionMatrix"));
    params[shader].emplace_back(glGetUniformLocation(shader,"viewMatrix"));
    params[shader].emplace_back(glGetUniformLocation(shader,"transformMatrix"));
    params[shader].emplace_back(glGetUniformLocation(shader,"time"));

    for(const auto& uniform : uniforms)
    {
        params[shader].emplace_back(glGetUniformLocation(shader,uniform.c_str()));
        //TODO: check uniform not found
    }
}

GLuint loadShader(string materialName,const list<string>& uniforms)
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
    loadShaderUniforms(uniforms,programID);
    return programID;
}

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

namespace Scene {
    mat4 projectionMatrix, viewMatrix;
    const float fov = 80.0;
    float time = 0.0;
    vec3 focusOrigin;

    void update()
    {
        projectionMatrix = glm::perspective(glm::radians(fov), 800.0f / 600.0f, 0.1f, 100.0f);
        
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
        if(currentShader == -1) return;

        glUniformMatrix4fv(params[currentShader][PROJECTION_MATRIX],1,false,&projectionMatrix[0][0]);
        glUniformMatrix4fv(params[currentShader][VIEW_MATRIX],1,false,&viewMatrix[0][0]);
        glUniform1f(params[currentShader][TIME],time);
    }
};
void useShader(GLuint programID)
{
    if(currentShader != programID)
    {
        currentShader = programID;
        glUseProgram(currentShader);
        Scene::flush();
    }
}
struct Mesh {
    GLuint vao,vbo;
    GLuint programID;

    mat4 transformMatrix = mat4(1.0);
    int vertexCount;

    inline void draw()
    {
        useShader(programID);
        glUniformMatrix4fv(params[currentShader][TRANSFORM_MATRIX],1,false,&transformMatrix[0][0]);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES,0,vertexCount);
    }

    virtual void process() 
    {
        transformMatrix = rotate<float>(transformMatrix,deltaTime * M_PI * 0.5 * 0.1,vec3(0,1,0));
        transformMatrix = translate<float>(transformMatrix,vec3(5.0,0,0) * deltaTime);
    }
};

vector<Mesh> meshes;

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

    mesh.programID = shaders[0];
    return mesh;
}

int render_loop(Window* window)
{
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);  
    glEnable(GL_DEPTH_TEST);    
    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
    
    do{
        glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.0,0.0,0.0,1.0);

        Scene::time += deltaTime;
        Scene::update();
        Scene::flush();

        for(int i = 0; i < meshes.size(); i++)
        {
            meshes[i].process();
            meshes[i].draw();
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    
    }
    while( glfwGetKey(window, GLFW_KEY_ESCAPE ) != GLFW_PRESS &&
           glfwWindowShouldClose(window) == 0 );

    return 0;
}

void loadSpecificMaterials()
{
    loadShader("primitive",list<string>());
    loadShader("emissive",{"emissive"});
}
void loadSpecificWorld()
{
    //meshes.emplace_back(createPrimitiveMesh(Cube));
    vec3 midPoint = vec3(0.0);
    int l = 200;
    for (size_t i = 0; i < l; i++)
    {
        Mesh mesh = createPrimitiveMesh(Cube);
        int s = 100;
        vec3 randPos = vec3(rand() % s,rand() % s,rand() % s);
        midPoint += randPos;

        mesh.transformMatrix = translate(mesh.transformMatrix,randPos);
        meshes.push_back(mesh);
    }

    Scene::focusOrigin = midPoint * 1.0f / float(l);
    
}

int main(int argc, char** argv)
{
    Window *window = createWindow();
    
    glfwSetCursorPosCallback(window, Viewport::cursor_position_callback);
    glfwSetFramebufferSizeCallback(window, Viewport::framebuffer_size_callback);

    //Estas funciones cargan un mundo específico usnando toda la API
    loadSpecificMaterials();
    loadSpecificWorld();
    
    return render_loop(window);
}

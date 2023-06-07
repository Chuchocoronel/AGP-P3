//
// engine.h: This file contains the types and functions relative to the engine.
//

#pragma once
#include "platform.h"
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <glad/glad.h>
#include <stb_image.h>
#include <stb_image_write.h>
#include"Camera.h"
#include <vector>
#include <string>
#include <random>
#define BINDING(b) b
typedef glm::vec2  vec2;
typedef glm::vec3  vec3;
typedef glm::vec4  vec4;
typedef glm::ivec2 ivec2;
typedef glm::ivec3 ivec3;
typedef glm::ivec4 ivec4;
class Objects;
struct VertexBufferAttribute {
    u8 location;
    u8 componenetCount;
    u8 offset;
};
struct VertexBufferLayout {
    std::vector<VertexBufferAttribute> attributes;
    u8 stride;
};
struct Vao {
    GLuint handle;
    GLuint programHandle;
};
struct Model {
    u32 meshIdx;
    std::vector<u32> materialIdx;
};
struct Submesh {
    VertexBufferLayout vertexBufferLayout;
    std::vector<float> vertices;
    std::vector<u32> indices;
    u32 vertexOffset;
    u32 indexOffset;
    std::vector<Vao> vaos;

};
struct Mesh {
    std::vector<Submesh> submeshes;
    GLuint vertexBufferHandle;
    GLuint indexBufferHandle;
};
struct Material {
    std::string name;
    vec3 albedo;
    vec3 emissive;
    f32 smoothness;
    u32 albedoTextureIdx;
    u32 emissiveTextureIdx;
    u32 specularTextureIdx;
    u32 normalsTextureIdx;
    u32 bumpTextureIdx;
};
struct Image
{
    void* pixels;
    ivec2 size;
    i32   nchannels;
    i32   stride;
};

struct Texture
{
    GLuint      handle;
    std::string filepath;
};
struct VertexShaderAttribute
{
    u8 location;
    u8 componentCount;
};
struct VertexShaderLayout
{
    std::vector<VertexShaderAttribute> attributes;
};
struct Program
{
    GLuint             handle;
    std::string        filepath;
    std::string        programName;
    u64                lastWriteTimestamp; // What is this for?
    VertexShaderLayout vertexInputLayout;
};

enum Mode
{
    Mode_TexturedQuad,
    Mode_Count
};
struct OpenGlInfo {

    std::string glVversion;
    std::string glRender;
    std::string glVendor;
    std::string glShadingVersion;
    std::vector<std::string> glextensions;
};
struct Vertexv3v2 {
    glm::vec3 pos;
    glm::vec2 uv;
};
const Vertexv3v2 vertices[] = {
    {glm::vec3(-0.5,-0.5,0.0),glm::vec2(0.0,0.0)},
    {glm::vec3(0.5,-0.5,0.0),glm::vec2(1.0,0.0)},
    {glm::vec3(0.5,0.5,0.0),glm::vec2(1.0,1.0)},
    {glm::vec3(-0.5,0.5,0.0),glm::vec2(0.0,1.0)},
};
const u16 indices[] = {
    0,1,2,
    0,2,3
};
class Buffer {
public:
    u32 size;
    GLenum type;
    GLuint handle;
    int head;
    u8* data;

};

enum LightType {
    Directional,
    Point,
    Spot
};
class Light {
public:
    LightType type;
    vec3 color;
    vec3 direction;
    vec3 position;
    float intensity;
    float angle;
    Objects* meshAttached = nullptr;
    vec3 rot;
    vec3 lastRot;
};
class Objects {
public:
    Objects(vec3 Position = { 0,0,0 }, vec3 Scale = { 1,1,1 }, vec3 Rotation = { 0,0,0 }) {
        modelMat = glm::mat4(1.0f);
        position = Position;
        scale = Scale;
        rotation = Rotation;
        modelMat = glm::translate(modelMat, position);
        modelMat = glm::scale(modelMat, scale);

        modelMat = glm::rotate(modelMat, rotation.x, glm::vec3(1, 0, 0));
        modelMat = glm::rotate(modelMat, rotation.y, glm::vec3(0, 1, 0));
        modelMat = glm::rotate(modelMat, rotation.z, glm::vec3(0, 0, 1));
    }
    void updateTransform() {
        modelMat = glm::mat4(1.0f);

        modelMat = glm::translate(modelMat, position);
        modelMat = glm::scale(modelMat, scale);

        modelMat = glm::rotate(modelMat, rotation.x, glm::vec3(1, 0, 0));
        modelMat = glm::rotate(modelMat, rotation.y, glm::vec3(0, 1, 0));
        modelMat = glm::rotate(modelMat, rotation.z, glm::vec3(0, 0, 1));
    }
    int showInGeneralList;
    int shaderID;
    int meshID;
    glm::mat4 modelMat;
    vec3 position = { 0,0,0 };
    vec3 scale = { 1,1,1 };
    vec3 rotation = { 0,0,0 };
    Light* lightAttached=nullptr;
};
struct App
{

    std::vector<glm::vec3> ssaoKernel;

    //mesh
    int PatrickID;
    int PlaneID;
    int SphereID;
    int TourusID;
    //programs
    int LightID;
    int EmptyObjID;
    //
    std::vector<Objects*> sceneObjects;
    int globalParamsOffset;
    int globalParamsSize;
    int globalParamsOffsetSecond;
    int globalParamsSizeSecond;
    Buffer cbuffer;
    Buffer cbufferSecond;
    int selectedFrameBuffer = 6;
    const char* current_item="Final Render SSAO";
    const char* items[7] = { "Albedo", "Normal", "Position","Depth","ssao","Final Render NO SSAO","Final Render SSAO" };
    unsigned int gBuffer;
    unsigned int gPosition, gNormal, gAlbedoSpec,gDepth, ssao, ggPosition, ggNormal;
    /// ////////////////////////////
    /// ////////////////////////////
    std::vector<Texture> textures;
    std::vector<Material> materials;
    std::vector<Mesh> meshes;
    std::vector<Model> models;
    std::vector<Program> programs;
    // Loop
    f32  deltaTime;
    bool isRunning;
    //lights
    std::vector<Light*>lights;
    
    // Input
    Input input;
    
    // Graphics
    char gpuName[64];
    char openGlVersion[64];
    OpenGlInfo glinfo;
    ivec2 displaySize;

    Camera* camera;
    // program indices
    u32 texturedGeometryProgramIdx;
    
    u32 texturedQuadProgramIdx;
    // texture indices
    u32 diceTexIdx;
    u32 whiteTexIdx;
    u32 blackTexIdx;
    u32 normalTexIdx;
    u32 magentaTexIdx;

    // Mode
    Mode mode;
    unsigned int VBO, VAO, EBO;
    unsigned int ext;
    // Embedded geometry (in-editor simple meshes such as
    // a screen filling quad, a cube, a sphere...)
    GLuint embeddedVertices;
    GLuint embeddedElements;

    // Location of the texture uniform in the textured quad shader
    // VAO object to link our screen filling quad with our textured quad shader
};
u32 LoadTexture2D(App* app, const char* filepath);
void Init(App* app);

void Gui(App* app);

void Update(App* app);

void Render(App* app);

void ProcessAssimpMesh(const aiScene* scene, aiMesh* mesh, Mesh* myMesh, u32 baseMeshMaterialIndex, std::vector<u32>& submeshMaterialIndices);
void ProcessAssimpMaterial(App* app, aiMaterial* material, Material& myMaterial, String directory);
void ProcessAssimpNode(const aiScene* scene, aiNode* node, Mesh* myMesh, u32 baseMeshMaterialIndex, std::vector<u32>& submeshMaterialIndices);
u32 LoadModel(App* app, const char* filename);


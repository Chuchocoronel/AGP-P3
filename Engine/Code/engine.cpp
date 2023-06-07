//
// engine.cpp : Put all your graphics stuff in this file. This is kind of the graphics module.
// In here, you should type all your OpenGL commands, and you can also type code to handle
// input platform events (e.g to move the camera or react to certain shortcuts), writing some
// graphics related GUI options, and so on.
//

#include "engine.h"
#include <imgui.h>
////////////////////////////////////////
bool IsPowerOf2(u32 value)
{
    return value && !(value & (value - 1));
}

u32 Align(u32 value, u32 alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

Buffer CreateBuffer(u32 size, GLenum type, GLenum usage)
{
    Buffer buffer = {};
    buffer.size = size;
    buffer.type = type;

    glGenBuffers(1, &buffer.handle);
    glBindBuffer(type, buffer.handle);
    glBufferData(type, buffer.size, NULL, usage);
    glBindBuffer(type, 0);

    return buffer;
}

#define CreateConstantBuffer(size) CreateBuffer(size, GL_UNIFORM_BUFFER, GL_STREAM_DRAW)
#define CreateStaticVertexBuffer(size) CreateBuffer(size, GL_ARRAY_BUFFER, GL_STATIC_DRAW)
#define CreateStaticIndexBuffer(size) CreateBuffer(size, GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW)

void BindBuffer(const Buffer& buffer)
{
    glBindBuffer(buffer.type, buffer.handle);
}

void MapBuffer(Buffer& buffer, GLenum access)
{
    glBindBuffer(buffer.type, buffer.handle);
    buffer.data = (u8*)glMapBuffer(buffer.type, access);
    buffer.head = 0;
}

void UnmapBuffer(Buffer& buffer)
{
    glUnmapBuffer(buffer.type);
    glBindBuffer(buffer.type, 0);
}

void AlignHead(Buffer& buffer, u32 alignment)
{
    ASSERT(IsPowerOf2(alignment), "The alignment must be a power of 2");
    buffer.head = Align(buffer.head, alignment);
}

void PushAlignedData(Buffer& buffer, const void* data, u32 size, u32 alignment)
{
    ASSERT(buffer.data != NULL, "The buffer must be mapped first");
    AlignHead(buffer, alignment);
    memcpy((u8*)buffer.data + buffer.head, data, size);
    buffer.head += size;
}

#define PushData(buffer, data, size) PushAlignedData(buffer, data, size, 1)
#define PushUInt(buffer, value) { u32 v = value; PushAlignedData(buffer, &v, sizeof(v), 4); }
#define PushUFloat(buffer, value) { float v = value; PushAlignedData(buffer, &v, sizeof(v), sizeof(float)); }
#define PushVec3(buffer, value) PushAlignedData(buffer, value_ptr(value), sizeof(value), sizeof(vec4))
#define PushVec4(buffer, value) PushAlignedData(buffer, value_ptr(value), sizeof(value), sizeof(vec4))
#define PushMat3(buffer, value) PushAlignedData(buffer, value_ptr(value), sizeof(value), sizeof(vec4))
#define PushMat4(buffer, value) PushAlignedData(buffer, value_ptr(value), sizeof(value), sizeof(vec4))
////////////////////////////////////////

void ProcessAssimpMesh(const aiScene* scene, aiMesh* mesh, Mesh* myMesh, u32 baseMeshMaterialIndex, std::vector<u32>& submeshMaterialIndices)
{
    std::vector<float> vertices;
    std::vector<u32> indices;

    bool hasTexCoords = false;
    bool hasTangentSpace = false;

    // process vertices
    for (unsigned int i = 0; i < mesh->mNumVertices; i++)
    {
        vertices.push_back(mesh->mVertices[i].x);
        vertices.push_back(mesh->mVertices[i].y);
        vertices.push_back(mesh->mVertices[i].z);
        vertices.push_back(mesh->mNormals[i].x);
        vertices.push_back(mesh->mNormals[i].y);
        vertices.push_back(mesh->mNormals[i].z);

        if (mesh->mTextureCoords[0]) // does the mesh contain texture coordinates?
        {
            hasTexCoords = true;
            vertices.push_back(mesh->mTextureCoords[0][i].x);
            vertices.push_back(mesh->mTextureCoords[0][i].y);
        }

        if (mesh->mTangents != nullptr && mesh->mBitangents)
        {
            hasTangentSpace = true;
            vertices.push_back(mesh->mTangents[i].x);
            vertices.push_back(mesh->mTangents[i].y);
            vertices.push_back(mesh->mTangents[i].z);

            // For some reason ASSIMP gives me the bitangents flipped.
            // Maybe it's my fault, but when I generate my own geometry
            // in other files (see the generation of standard assets)
            // and all the bitangents have the orientation I expect,
            // everything works ok.
            // I think that (even if the documentation says the opposite)
            // it returns a left-handed tangent space matrix.
            // SOLUTION: I invert the components of the bitangent here.
            vertices.push_back(-mesh->mBitangents[i].x);
            vertices.push_back(-mesh->mBitangents[i].y);
            vertices.push_back(-mesh->mBitangents[i].z);
        }
    }

    // process indices
    for (unsigned int i = 0; i < mesh->mNumFaces; i++)
    {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++)
        {
            indices.push_back(face.mIndices[j]);
        }
    }

    // store the proper (previously proceessed) material for this mesh
    submeshMaterialIndices.push_back(baseMeshMaterialIndex + mesh->mMaterialIndex);

    // create the vertex format
    VertexBufferLayout vertexBufferLayout = {};
    vertexBufferLayout.attributes.push_back(VertexBufferAttribute{ 0, 3, 0 });
    vertexBufferLayout.attributes.push_back(VertexBufferAttribute{ 1, 3, 3 * sizeof(float) });
    vertexBufferLayout.stride = 6 * sizeof(float);
    if (hasTexCoords)
    {
        vertexBufferLayout.attributes.push_back(VertexBufferAttribute{ 2, 2, vertexBufferLayout.stride });
        vertexBufferLayout.stride += 2 * sizeof(float);
    }
    if (hasTangentSpace)
    {
        vertexBufferLayout.attributes.push_back(VertexBufferAttribute{ 3, 3, vertexBufferLayout.stride });
        vertexBufferLayout.stride += 3 * sizeof(float);

        vertexBufferLayout.attributes.push_back(VertexBufferAttribute{ 4, 3, vertexBufferLayout.stride });
        vertexBufferLayout.stride += 3 * sizeof(float);
    }

    // add the submesh into the mesh
    Submesh submesh = {};
    submesh.vertexBufferLayout = vertexBufferLayout;
    submesh.vertices.swap(vertices);
    submesh.indices.swap(indices);
    myMesh->submeshes.push_back(submesh);
}

void ProcessAssimpMaterial(App* app, aiMaterial* material, Material& myMaterial, String directory)
{
    aiString name;
    aiColor3D diffuseColor;
    aiColor3D emissiveColor;
    aiColor3D specularColor;
    ai_real shininess;
    material->Get(AI_MATKEY_NAME, name);
    material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColor);
    material->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveColor);
    material->Get(AI_MATKEY_COLOR_SPECULAR, specularColor);
    material->Get(AI_MATKEY_SHININESS, shininess);

    myMaterial.name = name.C_Str();
    myMaterial.albedo = vec3(diffuseColor.r, diffuseColor.g, diffuseColor.b);
    myMaterial.emissive = vec3(emissiveColor.r, emissiveColor.g, emissiveColor.b);
    myMaterial.smoothness = shininess / 256.0f;

    aiString aiFilename;
    if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0)
    {
        material->GetTexture(aiTextureType_DIFFUSE, 0, &aiFilename);
        String filename = MakeString(aiFilename.C_Str());
        String filepath = MakePath(directory, filename);
        myMaterial.albedoTextureIdx = LoadTexture2D(app, filepath.str);
    }
    if (material->GetTextureCount(aiTextureType_EMISSIVE) > 0)
    {
        material->GetTexture(aiTextureType_EMISSIVE, 0, &aiFilename);
        String filename = MakeString(aiFilename.C_Str());
        String filepath = MakePath(directory, filename);
        myMaterial.emissiveTextureIdx = LoadTexture2D(app, filepath.str);
    }
    if (material->GetTextureCount(aiTextureType_SPECULAR) > 0)
    {
        material->GetTexture(aiTextureType_SPECULAR, 0, &aiFilename);
        String filename = MakeString(aiFilename.C_Str());
        String filepath = MakePath(directory, filename);
        myMaterial.specularTextureIdx = LoadTexture2D(app, filepath.str);
    }
    if (material->GetTextureCount(aiTextureType_NORMALS) > 0)
    {
        material->GetTexture(aiTextureType_NORMALS, 0, &aiFilename);
        String filename = MakeString(aiFilename.C_Str());
        String filepath = MakePath(directory, filename);
        myMaterial.normalsTextureIdx = LoadTexture2D(app, filepath.str);
    }
    if (material->GetTextureCount(aiTextureType_HEIGHT) > 0)
    {
        material->GetTexture(aiTextureType_HEIGHT, 0, &aiFilename);
        String filename = MakeString(aiFilename.C_Str());
        String filepath = MakePath(directory, filename);
        myMaterial.bumpTextureIdx = LoadTexture2D(app, filepath.str);
    }

    //myMaterial.createNormalFromBump();
}

void ProcessAssimpNode(const aiScene* scene, aiNode* node, Mesh* myMesh, u32 baseMeshMaterialIndex, std::vector<u32>& submeshMaterialIndices)
{
    // process all the node's meshes (if any)
    for (unsigned int i = 0; i < node->mNumMeshes; i++)
    {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        ProcessAssimpMesh(scene, mesh, myMesh, baseMeshMaterialIndex, submeshMaterialIndices);
    }

    // then do the same for each of its children
    for (unsigned int i = 0; i < node->mNumChildren; i++)
    {
        ProcessAssimpNode(scene, node->mChildren[i], myMesh, baseMeshMaterialIndex, submeshMaterialIndices);
    }
}

u32 LoadModel(App* app, const char* filename)
{
    const aiScene* scene = aiImportFile(filename,
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_JoinIdenticalVertices |
        aiProcess_PreTransformVertices |
        aiProcess_ImproveCacheLocality |
        aiProcess_OptimizeMeshes |
        aiProcess_SortByPType);

    /* if (!scene)
     {
         ELOG("Error loading mesh %s: %s", filename, aiGetErrorString());
         return UINT32_MAX;
     }*/

    app->meshes.push_back(Mesh{});
    Mesh& mesh = app->meshes.back();
    u32 meshIdx = (u32)app->meshes.size() - 1u;

    app->models.push_back(Model{});
    Model& model = app->models.back();
    model.meshIdx = meshIdx;
    u32 modelIdx = (u32)app->models.size() - 1u;

    String directory = GetDirectoryPart(MakeString(filename));

    // Create a list of materials
    u32 baseMeshMaterialIndex = (u32)app->materials.size();
    for (unsigned int i = 0; i < scene->mNumMaterials; ++i)
    {
        app->materials.push_back(Material{});
        Material& material = app->materials.back();
        ProcessAssimpMaterial(app, scene->mMaterials[i], material, directory);
    }

    ProcessAssimpNode(scene, scene->mRootNode, &mesh, baseMeshMaterialIndex, model.materialIdx);

    aiReleaseImport(scene);

    u32 vertexBufferSize = 0;
    u32 indexBufferSize = 0;

    for (u32 i = 0; i < mesh.submeshes.size(); ++i)
    {
        vertexBufferSize += mesh.submeshes[i].vertices.size() * sizeof(float);
        indexBufferSize += mesh.submeshes[i].indices.size() * sizeof(u32);
    }

    glGenBuffers(1, &mesh.vertexBufferHandle);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vertexBufferHandle);
    glBufferData(GL_ARRAY_BUFFER, vertexBufferSize, NULL, GL_STATIC_DRAW);

    glGenBuffers(1, &mesh.indexBufferHandle);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indexBufferHandle);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexBufferSize, NULL, GL_STATIC_DRAW);

    u32 indicesOffset = 0;
    u32 verticesOffset = 0;

    for (u32 i = 0; i < mesh.submeshes.size(); ++i)
    {
        const void* verticesData = mesh.submeshes[i].vertices.data();
        const u32   verticesSize = mesh.submeshes[i].vertices.size() * sizeof(float);
        glBufferSubData(GL_ARRAY_BUFFER, verticesOffset, verticesSize, verticesData);
        mesh.submeshes[i].vertexOffset = verticesOffset;
        verticesOffset += verticesSize;

        const void* indicesData = mesh.submeshes[i].indices.data();
        const u32   indicesSize = mesh.submeshes[i].indices.size() * sizeof(u32);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, indicesOffset, indicesSize, indicesData);
        mesh.submeshes[i].indexOffset = indicesOffset;
        indicesOffset += indicesSize;
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return modelIdx;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
GLuint CreateProgramFromSource(String programSource, const char* shaderName)
{
    GLchar  infoLogBuffer[1024] = {};
    GLsizei infoLogBufferSize = sizeof(infoLogBuffer);
    GLsizei infoLogSize;
    GLint   success;

    char versionString[] = "#version 430\n";
    char shaderNameDefine[128];
    sprintf_s(shaderNameDefine, "#define %s\n", shaderName);
    char vertexShaderDefine[] = "#define VERTEX\n";
    char fragmentShaderDefine[] = "#define FRAGMENT\n";

    const GLchar* vertexShaderSource[] = {
        versionString,
        shaderNameDefine,
        vertexShaderDefine,
        programSource.str
    };
    const GLint vertexShaderLengths[] = {
        (GLint) strlen(versionString),
        (GLint) strlen(shaderNameDefine),
        (GLint) strlen(vertexShaderDefine),
        (GLint) programSource.len
    };
    const GLchar* fragmentShaderSource[] = {
        versionString,
        shaderNameDefine,
        fragmentShaderDefine,
        programSource.str
    };
    const GLint fragmentShaderLengths[] = {
        (GLint) strlen(versionString),
        (GLint) strlen(shaderNameDefine),
        (GLint) strlen(fragmentShaderDefine),
        (GLint) programSource.len
    };

    GLuint vshader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vshader, ARRAY_COUNT(vertexShaderSource), vertexShaderSource, vertexShaderLengths);
    glCompileShader(vshader);
    glGetShaderiv(vshader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vshader, infoLogBufferSize, &infoLogSize, infoLogBuffer);
        ELOG("glCompileShader() failed with vertex shader %s\nReported message:\n%s\n", shaderName, infoLogBuffer);
    }

    GLuint fshader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fshader, ARRAY_COUNT(fragmentShaderSource), fragmentShaderSource, fragmentShaderLengths);
    glCompileShader(fshader);
    glGetShaderiv(fshader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fshader, infoLogBufferSize, &infoLogSize, infoLogBuffer);
        ELOG("glCompileShader() failed with fragment shader %s\nReported message:\n%s\n", shaderName, infoLogBuffer);
    }

    GLuint programHandle = glCreateProgram();
    glAttachShader(programHandle, vshader);
    glAttachShader(programHandle, fshader);
    glLinkProgram(programHandle);
    glGetProgramiv(programHandle, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(programHandle, infoLogBufferSize, &infoLogSize, infoLogBuffer);
        ELOG("glLinkProgram() failed with program %s\nReported message:\n%s\n", shaderName, infoLogBuffer);
    }

    glUseProgram(0);

    glDetachShader(programHandle, vshader);
    glDetachShader(programHandle, fshader);
    glDeleteShader(vshader);
    glDeleteShader(fshader);

    return programHandle;
}

u32 LoadProgram(App* app, const char* filepath, const char* programName)
{
    String programSource = ReadTextFile(filepath);

    Program program = {};
    program.handle = CreateProgramFromSource(programSource, programName);
    program.filepath = filepath;
    program.programName = programName;
    program.lastWriteTimestamp = GetFileLastWriteTimestamp(filepath);
    app->programs.push_back(program);

    return app->programs.size() - 1;
}

Image LoadImage(const char* filename)
{
    Image img = {};
    stbi_set_flip_vertically_on_load(true);
    img.pixels = stbi_load(filename, &img.size.x, &img.size.y, &img.nchannels, 0);
    if (img.pixels)
    {
        img.stride = img.size.x * img.nchannels;
    }
    else
    {
        ELOG("Could not open file %s", filename);
    }
    return img;
}

void FreeImage(Image image)
{
    stbi_image_free(image.pixels);
}

GLuint CreateTexture2DFromImage(Image image)
{
    GLenum internalFormat = GL_RGB8;
    GLenum dataFormat     = GL_RGB;
    GLenum dataType       = GL_UNSIGNED_BYTE;

    switch (image.nchannels)
    {
        case 3: dataFormat = GL_RGB; internalFormat = GL_RGB8; break;
        case 4: dataFormat = GL_RGBA; internalFormat = GL_RGBA8; break;
        default: ELOG("LoadTexture2D() - Unsupported number of channels");
    }

    GLuint texHandle;
    glGenTextures(1, &texHandle);
    glBindTexture(GL_TEXTURE_2D, texHandle);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, image.size.x, image.size.y, 0, dataFormat, dataType, image.pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    return texHandle;
}

u32 LoadTexture2D(App* app, const char* filepath)
{
    for (u32 texIdx = 0; texIdx < app->textures.size(); ++texIdx)
        if (app->textures[texIdx].filepath == filepath)
            return texIdx;

    Image image = LoadImage(filepath);

    if (image.pixels)
    {
        Texture tex = {};
        tex.handle = CreateTexture2DFromImage(image);
        tex.filepath = filepath;

        u32 texIdx = app->textures.size();
        app->textures.push_back(tex);

        FreeImage(image);
        return texIdx;
    }
    else
    {
        return UINT32_MAX;
    }
}
void initGBuffer(App* app) {
    
    glGenFramebuffers(1, &app->gBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, app->gBuffer);
    
    glGenTextures(1, &app->gPosition);
    glBindTexture(GL_TEXTURE_2D, app->gPosition);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, app->displaySize.x, app->displaySize.y, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, app->gPosition, 0);
    // normal
    glGenTextures(1, &app->gNormal);
    glBindTexture(GL_TEXTURE_2D, app->gNormal);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, app->displaySize.x, app->displaySize.y, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, app->gNormal, 0);
    
    // position
    
    // color + specular
    glGenTextures(1, &app->gAlbedoSpec);
    glBindTexture(GL_TEXTURE_2D, app->gAlbedoSpec);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, app->displaySize.x, app->displaySize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, app->gAlbedoSpec, 0);

   
    // depth buffer
    /*unsigned int rboDepth;
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, app->displaySize.x, app->displaySize.y);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);


    glGenTextures(1, &app->gDepth);
    glBindTexture(GL_TEXTURE_2D, app->gDepth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, app->displaySize.x, app->displaySize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, app->gDepth, 0);*/



    unsigned int rboDepth;
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, app->displaySize.x, app->displaySize.y);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);


    glGenTextures(1, &app->gDepth);
    glBindTexture(GL_TEXTURE_2D, app->gDepth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, app->displaySize.x, app->displaySize.y, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, app->gDepth, 0);

    glGenTextures(1, &app->ggPosition);
    glBindTexture(GL_TEXTURE_2D, app->ggPosition);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, app->displaySize.x, app->displaySize.y, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_2D, app->ggPosition, 0);
    // normal
    glGenTextures(1, &app->ggNormal);
    glBindTexture(GL_TEXTURE_2D, app->ggNormal);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, app->displaySize.x, app->displaySize.y, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT5, GL_TEXTURE_2D, app->ggNormal, 0);
    
    // tell OpenGL which color attachments we'll use (of this framebuffer) for rendering 
    unsigned int attachments[6] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5 };
    glDrawBuffers(6, attachments);
    
    /////////////////////////////////////////////////////
    std::vector<glm::vec3> ssaoNoise;
    std::uniform_real_distribution<float> randomFloats(0.0, 1.0);
    std::default_random_engine generator;
    for (unsigned int i = 0; i < 16; i++)
    {
        glm::vec3 noise(
            randomFloats(generator) * 2.0 - 1.0,
            randomFloats(generator) * 2.0 - 1.0,
            0.0f);
        ssaoNoise.push_back(noise);
    }

    /*glGenTextures(1, &app->ssao);
    glBindTexture(GL_TEXTURE_2D, app->ssao);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 4, 4, 0, GL_RGB, GL_FLOAT, &ssaoNoise[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);*/
    


    

    // finally check if framebuffer is complete
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    Program& texturedMeshPRogram = app->programs[app->texturedQuadProgramIdx];
    glUseProgram(texturedMeshPRogram.handle);
    glUniform1i(glGetUniformLocation(texturedMeshPRogram.handle, "gPosition"), 0);
    glUniform1i(glGetUniformLocation(texturedMeshPRogram.handle, "gNormal"), 1);
    glUniform1i(glGetUniformLocation(texturedMeshPRogram.handle, "gAlbedoSpec"), 2);
    glUniform1i(glGetUniformLocation(texturedMeshPRogram.handle, "gDepth"), 3);
    glUniform1i(glGetUniformLocation(texturedMeshPRogram.handle, "ggPosition"),4);
    glUniform1i(glGetUniformLocation(texturedMeshPRogram.handle, "ggNormal"), 5);
}
void initFrontPlane(App* app) {
    float quadVertices[] = {
        // positions        // texture Coords
        -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
         1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
         1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
    };
    // setup plane VAO
    app->texturedQuadProgramIdx = LoadProgram(app, "quad.glsl", "TEXTURED_QUAD");
    Program& texturedMeshProgram = app->programs[app->texturedQuadProgramIdx];
    glGenVertexArrays(1, &app->VAO);
    glGenBuffers(1, &app->VBO);
    glBindVertexArray(app->VAO);
    glBindBuffer(GL_ARRAY_BUFFER, app->VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    
}
Objects* CreateObject(App* app, int objectIndex, vec3 postion = { 0,0,0 }, vec3 scale = { 1,1,1 }, vec3 rotation = { 0,0,0 }, bool partOfGeneralList = true)
{
    Objects* ob1 = new Objects(postion, scale, rotation);
    ob1->showInGeneralList = partOfGeneralList;
    switch (objectIndex)
    {
        //patrick
    case 0:
        ob1->meshID = app->PatrickID;
        ob1->shaderID = app->LightID;
        break;
    case 1:
        ob1->meshID = app->PlaneID;
        ob1->shaderID = app->EmptyObjID;
        break;
    case 2:
        ob1->meshID = app->SphereID;
        ob1->shaderID = app->EmptyObjID; 
        break;
    case 3:
        ob1->meshID = app->TourusID;
        ob1->shaderID = app->EmptyObjID; 
        break;
    default:
        break;
    }
    app->sceneObjects.push_back(ob1);
    return ob1;
}
void DestroyObject(App* app, Objects* position)
{
    
    app->sceneObjects.erase(std::remove(app->sceneObjects.begin(), app->sceneObjects.end(), position), app->sceneObjects.end());
    delete position;

}
void CreateLight(App* app, LightType type, vec3 postion = { 0,2,0 }, vec3 color = { 1,1,1 }, float intensity = 1) {
    Light* l1=new Light();
    l1->color = color;
    l1->direction = vec3(1, 1, 1);
    l1->position = postion;
    
    l1->intensity = intensity;
    l1->angle = 20.0f;

    switch (type)
    {
    case Directional:
        l1->direction = vec3(0, 6.3f, 0);
        
        l1->meshAttached = CreateObject(app, 1, l1->position, { 0.3f,0.3f,0.3f }, { 0,0,0 }, false);

        app->sceneObjects[app->sceneObjects.size() - 1]->lightAttached = l1;
        l1->type = type;
        break;
    case Point:
        
        l1->meshAttached= CreateObject(app, 2, l1->position, { 0.1f,0.1f,0.1f }, { 0,0,0 }, false);
        app->sceneObjects[app->sceneObjects.size() - 1]->lightAttached = l1;
        l1->type = type;
        break;
    case Spot:
        l1->type = type;
        break;
    default:
        break;
    }
    app->lights.push_back(l1);
}
void DestroyLight(App* app,Light* position) {
    
    if (position->meshAttached!=nullptr) {
        DestroyObject(app, position->meshAttached);
    }
    
    app->lights.erase(std::remove(app->lights.begin(), app->lights.end(), position), app->lights.end());
    delete position;
    
}
float lerp(float a, float b, float f)
{
    return (a * (1.0 - f)) + (b * f);
}
void initRandomFloats(App* app)
{
    std::uniform_real_distribution<float> randomFloats(0.0, 1.0);
    std::default_random_engine generator;
    
    for (unsigned int i = 0; i < 64; ++i)
    {
        glm::vec3 sample(
            randomFloats(generator) * 2.0 - 1.0,
            randomFloats(generator) * 2.0 - 1.0,
            randomFloats(generator)
        );
        sample = glm::normalize(sample);
        sample *= randomFloats(generator);
        float scale = (float)i / 64.0;
      
        scale = lerp(0.1f, 1.0f, scale * scale);
        sample *= scale;

        app->ssaoKernel.push_back(sample);
    }


}
void Init(App* app)
{
    GLint maxUniformBufferSize;

    glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &maxUniformBufferSize);
    app->cbuffer = CreateBuffer(maxUniformBufferSize, GL_UNIFORM_BUFFER, GL_DYNAMIC_DRAW);
    app->cbufferSecond = CreateBuffer(maxUniformBufferSize, GL_UNIFORM_BUFFER, GL_DYNAMIC_DRAW);
    


    initFrontPlane(app);
    initGBuffer(app);
    initRandomFloats(app);
    app->camera= new Camera({-1.7,1.6f,16},{0,1,0});
    
    
    app->PatrickID= LoadModel(app, "Patrick/Patrick.obj");
    app->PlaneID = LoadModel(app, "Plane/Plane.obj");
    app->SphereID = LoadModel(app, "Sphere/Sphere.obj");
    app->TourusID = LoadModel(app, "Tourus/Tourus.obj");
   
    app->LightID = LoadProgram(app, "shaders.glsl","TEXTURED_GEOMETRY");
    Program& texturedMeshProgram = app->programs[app->LightID];
    texturedMeshProgram.vertexInputLayout.attributes.push_back({ 3,5 });
    texturedMeshProgram.vertexInputLayout.attributes.push_back({ 4,4 });
    texturedMeshProgram.vertexInputLayout.attributes.push_back({ 0,3 });
    texturedMeshProgram.vertexInputLayout.attributes.push_back({ 2,2 });
    texturedMeshProgram.vertexInputLayout.attributes.push_back({ 1,1 }); //TEXTURED_EMPTYOBJ
    

    app->EmptyObjID= LoadProgram(app, "EmptyObj.glsl", "TEXTURED_EMPTYOBJ");//
    Program& EmptyObjProgram = app->programs[app->EmptyObjID];
    EmptyObjProgram.vertexInputLayout.attributes.push_back({ 3,5 });
    EmptyObjProgram.vertexInputLayout.attributes.push_back({ 4,4 });
    EmptyObjProgram.vertexInputLayout.attributes.push_back({ 0,3 });
    EmptyObjProgram.vertexInputLayout.attributes.push_back({ 2,2 });
    EmptyObjProgram.vertexInputLayout.attributes.push_back({ 1,1 }); //TEXTURED_EMPTYOBJ


    CreateLight(app,LightType::Point,  {  2,2,2 }, { 1,0,0 },4);
    CreateLight(app, LightType::Point, { -2,2,2 }, { 0,1,0 }, 4);
    CreateLight(app, LightType::Point, { 2,2,-2 }, { 0,0,1 }, 4);
    CreateLight(app, LightType::Point, { -2,2,-2 }, { 1,0,1 }, 4);
    CreateLight(app, LightType::Directional, { 0,-2,0 }, { 1,1,1 }, 0.7f);

    CreateObject(app, 0,{-5, 0, 0},{1,1,1},{0,1.5f,0});
    CreateObject(app, 0,{5,0,0}, { 1,1,1 }, { 0,-1.5f,0 });
    CreateObject(app, 0, { 0, 0, -5 });
    CreateObject(app, 0, { 0,0,5 }, { 1,1,1 }, { 0,3.0f,0 });
    CreateObject(app, 1, { 0,-3.4f,0 }, { 15,1,15 });
    CreateObject(app, 2);
    CreateObject(app, 3, { 0,3.5f,0 });
    ///////////////////////////////////////////////////////////
    app->glinfo.glVversion = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    app->glinfo.glRender = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    app->glinfo.glShadingVersion = reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));
    app->glinfo.glVendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));


    GLint numExtensions = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
    for (int a = 0; a < numExtensions; a++) {
        app->glinfo.glextensions.push_back(reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS,GLuint(a))));
    }
    app->mode = Mode_TexturedQuad;
}
glm::vec3 rotateVector(const glm::vec3 axis, double angle, const glm::vec3 vector) {
    double radianAngle = angle * PI / 180.0;

    double cosAngle = cos(radianAngle);
    double sinAngle = sin(radianAngle);

    glm::vec3 rotatedVector;
    rotatedVector.x =
        (cosAngle + (1 - cosAngle) * axis.x * axis.x) * vector.x +
        ((1 - cosAngle) * axis.x * axis.y - sinAngle * axis.z) * vector.y +
        ((1 - cosAngle) * axis.x * axis.z + sinAngle * axis.y) * vector.z;

    rotatedVector.y =
        ((1 - cosAngle) * axis.y * axis.x + sinAngle * axis.z) * vector.x +
        (cosAngle + (1 - cosAngle) * axis.y * axis.y) * vector.y +
        ((1 - cosAngle) * axis.y * axis.z - sinAngle * axis.x) * vector.z;

    rotatedVector.z =
        ((1 - cosAngle) * axis.z * axis.x - sinAngle * axis.y) * vector.x +
        ((1 - cosAngle) * axis.z * axis.y + sinAngle * axis.x) * vector.y +
        (cosAngle + (1 - cosAngle) * axis.z * axis.z) * vector.z;

    return rotatedVector;
}
void Gui(App* app)
{
    ImGui::Begin("Info");
    ImGui::Text("FPS: %f", 1.0f/app->deltaTime);
    if (ImGui::CollapsingHeader("Final Render"))
    {
        ImGui::TextColored({ 1,0,0,1 }, "Final Render Texture");
        if (ImGui::BeginCombo("##custom combo", app->current_item, ImGuiComboFlags_NoArrowButton))
        {
            for (int n = 0; n < 7; n++)
            {
                bool is_selected = (app->current_item == app->items[n]);
                if (ImGui::Selectable(app->items[n], is_selected)) {
                    app->current_item = app->items[n];
                    app->selectedFrameBuffer = n;
                }


                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }
    if (ImGui::CollapsingHeader("Objects"))
    {
        if (ImGui::Button("Create Patrick")) {
            CreateObject(app, 0);
        }
        if (ImGui::Button("Create Plane")) {
            CreateObject(app, 1);
        }
        if (ImGui::Button("Create Sphere")) {
            CreateObject(app, 2);
        }
        if (ImGui::Button("Create Tourus")) {
            CreateObject(app, 3);
        }
        std::string ObjectsName;
        for (int a = 0; a < app->sceneObjects.size(); a++) 
        {
            if (app->sceneObjects[a]->showInGeneralList) {


                ObjectsName = "Object ";
                ObjectsName += std::to_string(a);
                if (ImGui::CollapsingHeader(ObjectsName.c_str()))
                {
                    bool matrixChange = false;
                    ImGui::TextColored({ 1,0,0,1 }, "Position");
                    ObjectsName = "Object Position ";
                    ObjectsName += std::to_string(a);
                    if (ImGui::DragFloat3(ObjectsName.c_str(), &app->sceneObjects[a]->position.x, 0.1f)) {
                        matrixChange = true;
                    }

                    /// ///////////////////////////////////////////////////
                    ImGui::TextColored({ 1,0,0,1 }, "Scale");
                    ObjectsName = "Object Scale ";
                    ObjectsName += std::to_string(a);
                    if (ImGui::DragFloat3(ObjectsName.c_str(), &app->sceneObjects[a]->scale.x, 0.1f)) {
                        matrixChange = true;
                    }

                    /// ///////////////////////////////////////////////////
                    ImGui::TextColored({ 1,0,0,1 }, "Rotation");
                    ObjectsName = "Object Rotation ";
                    ObjectsName += std::to_string(a);
                    if (ImGui::DragFloat3(ObjectsName.c_str(), &app->sceneObjects[a]->rotation.x, 0.1f)) {
                        matrixChange = true;
                    }
                    /// ///////////////////////////////////////////////////
                    ObjectsName = "Destroy Object ";
                    ObjectsName += std::to_string(a);
                    if (ImGui::Button(ObjectsName.c_str())) {
                        DestroyObject(app, app->sceneObjects[a]);
                    }
                    if (matrixChange) {
                        app->sceneObjects[a]->modelMat = glm::mat4(1.0f);

                        app->sceneObjects[a]->modelMat = glm::translate(app->sceneObjects[a]->modelMat, app->sceneObjects[a]->position);
                        app->sceneObjects[a]->modelMat = glm::scale(app->sceneObjects[a]->modelMat, app->sceneObjects[a]->scale);

                        app->sceneObjects[a]->modelMat = glm::rotate(app->sceneObjects[a]->modelMat, app->sceneObjects[a]->rotation.x, glm::vec3(1, 0, 0));
                        app->sceneObjects[a]->modelMat = glm::rotate(app->sceneObjects[a]->modelMat, app->sceneObjects[a]->rotation.y, glm::vec3(0, 1, 0));
                        app->sceneObjects[a]->modelMat = glm::rotate(app->sceneObjects[a]->modelMat, app->sceneObjects[a]->rotation.z, glm::vec3(0, 0, 1));
                    }
                }
            }
        }
        
    }
    if (ImGui::CollapsingHeader("Camera"))
    {
        ImGui::DragFloat3("Camera Position", &app->camera->Position.x, 0.1f);
        ImGui::DragFloat("Camera FOV", &app->camera->FOV, 0.3f);
        ImGui::DragFloat("Camera Near Plane", &app->camera->nearP,0.3f);
        ImGui::DragFloat("Camera Far Plane", &app->camera->farP, 0.3f);
        ImGui::DragFloat3("up", &app->camera->Up.x, 0.3f);
        ImGui::DragFloat3("front", &app->camera->Front.x, 0.3f);
        app->camera->CalculatePrjection();
    }
    if (ImGui::CollapsingHeader("Lights")) 
    {
        std::string positionName;
        std::string lightType;
        if (ImGui::Button("Create Point Light")) {
            CreateLight(app,LightType::Point);
        }
        if (ImGui::Button("Create Directional Light")) {
            CreateLight(app, LightType::Directional);
        }
        /*if (ImGui::Button("Create Spot Light")) {
            CreateLight(app, LightType::Spot);
        }*/
        for (int a = 0; a < app->lights.size(); a++) 
        {
            
            positionName = "Light ";
            positionName += std::to_string(a);
            if (ImGui::CollapsingHeader(positionName.c_str()))
            {
            
                
                /// ///////////////////////////////////////////////////
                positionName = "Light Type ";
                positionName += std::to_string(a);
                ImGui::TextColored({ 1,0,0,1 }, "Type");
                
                switch (app->lights[a]->type)
                {
                case LightType::Directional:
                    lightType = "Directional";
                    break;
                case LightType::Point:
                    lightType = "Point";
                    break;
                case LightType::Spot:
                    lightType = "Spot";
                    break;
                default:
                    break;
                }
                ImGui::TextColored({ 1,1,1,1 }, lightType.c_str());
                

                /// ///////////////////////////////////////////////////

                if (app->lights[a]->type == LightType::Spot)
                {
                    ImGui::Separator();
                    ImGui::Separator();

                    positionName = "Light Direction ";
                    positionName += std::to_string(a);
                    ImGui::TextColored({ 1,0,0,1 }, "Direction");
                    if (ImGui::DragFloat3(positionName.c_str(), &app->lights[a]->direction.x, 0.1f)) 
                    {

                       app->lights[a]->meshAttached->position = app->lights[a]->position;
                       app->lights[a]->meshAttached->updateTransform();
                        
                    }
                    
                    /// ///////////////////////////////////////////////////
                    ImGui::Separator();
                    ImGui::Separator();

                    positionName = "Light Angle ";
                    positionName += std::to_string(a);
                    ImGui::TextColored({ 1,0,0,1 }, "Angle");
                    ImGui::DragFloat(positionName.c_str(), &app->lights[a]->angle, 0.1f);
                }
                else if (app->lights[a]->type == LightType::Directional) 
                {
                    /// ///////////////////////////////////////////////////
                    ImGui::Separator();
                    ImGui::Separator();

                    positionName = "Light Direction ";
                    positionName += std::to_string(a);
                    ImGui::TextColored({ 1,0,0,1 }, "Direction");
                    if (ImGui::DragFloat3(positionName.c_str(), &app->lights[a]->rot.x, 0.1f))
                    {
                        vec3 e =  app->lights[a]->lastRot-app->lights[a]->rot;
                        app->lights[a]->direction = rotateVector({1,0,0}, e.x , app->lights[a]->direction);
                        app->lights[a]->direction = rotateVector({ 0,1,0 }, e.y, app->lights[a]->direction);
                        app->lights[a]->direction = rotateVector({ 0,0,1 }, e.z, app->lights[a]->direction);
                        app->lights[a]->lastRot=app->lights[a]->rot;
                        app->lights[a]->meshAttached->rotation=(-app->lights[a]->rot * PI) / 180.0f;
                        app->lights[a]->meshAttached->updateTransform();
                    }
                }
                /// ///////////////////////////////////////////////////
                ImGui::Separator();
                ImGui::Separator();

                positionName = "Light Position ";
                positionName += std::to_string(a);
                ImGui::TextColored({ 1,0,0,1 }, "Position");
                if (ImGui::DragFloat3(positionName.c_str(), &app->lights[a]->position.x, 0.1f)) {
                        app->lights[a]->meshAttached->position = app->lights[a]->position;
                        app->lights[a]->meshAttached->updateTransform();
                    
                    
                }
                /// ///////////////////////////////////////////////////
                ImGui::Separator();
                ImGui::Separator();

                positionName = "Light Intesity ";
                positionName += std::to_string(a);
                ImGui::TextColored({ 1,0,0,1 }, "Intesity");
                ImGui::DragFloat(positionName.c_str(), &app->lights[a]->intensity, 0.1f);
                /// ///////////////////////////////////////////////////
                ImGui::Separator();
                ImGui::Separator();
                
                positionName = "Light Color ";
                positionName += std::to_string(a);
                ImGui::TextColored({ 1,0,0,1 }, "Color");
                ImGui::ColorPicker3(positionName.c_str(), &app->lights[a]->color.x);
                /// ///////////////////////////////////////////////////
                positionName = "Destroy Light ";
                positionName += std::to_string(a);
                if (ImGui::Button(positionName.c_str())) {
                    DestroyLight(app, app->lights[a]);
                    break;
                }
                /// ///////////////////////////////////////////////////
            }
        }
    }
    
    
    
    ImGui::End();
   
    if (app->input.keys[Key::K_M] == ButtonState::BUTTON_PRESS) {
        ImGui::OpenPopup("opengl Info");
    }
    if (ImGui::BeginPopup("opengl Info")) 
    {
        ImGui::Text("version: %s", app->glinfo.glVversion);
        ImGui::Text("Renderer: %s", app->glinfo.glRender);
        ImGui::Text("Vendor: %s", app->glinfo.glVendor);
        ImGui::Text("glsl version: %s", app->glinfo.glShadingVersion);
        
        for (int a = 0; a < app->glinfo.glextensions.size(); a++) {

            ImGui::Text("glsl version: %s", app->glinfo.glextensions[a]);
        }
        ImGui::EndPopup();
    }
    
}
void processInput(App* app)
{
    if (app->input.keys[K_W] == BUTTON_PRESSED) {
        app->camera->ProcessKeyboard(FORWARD, app->deltaTime);

    }
    if (app->input.keys[K_S] == BUTTON_PRESSED) {
        app->camera->ProcessKeyboard(BACKWARD, app->deltaTime);

    }

    if (app->input.keys[K_D] == BUTTON_PRESSED) {

        app->camera->ProcessKeyboard(RRIGHT, app->deltaTime);
    }
    if (app->input.keys[K_A] == BUTTON_PRESSED) {
        app->camera->ProcessKeyboard(LLEFT, app->deltaTime);
    }
    if (app->input.mouseButtons[LEFT] == BUTTON_PRESSED) {
        app->camera->ProcessMouseMovement(app->input.mouseDelta.x, -app->input.mouseDelta.y);
    }

    if (app->input.mouseButtons[RIGHT] == BUTTON_PRESSED)
    {
        vec3 reference = { 0,0,0 };

        glm::quat pivot = glm::angleAxis(app->input.mouseDelta.x * 0.01f, app->camera->Up);

        if (abs(app->camera->Up.y) < 0.1f)
        {
            if ((app->camera->Position.y > reference.y && app->input.mouseDelta.y > 0.f) ||
                (app->camera->Position.y < reference.y && app->input.mouseDelta.y < 0.f))
                pivot = pivot * glm::angleAxis(app->input.mouseDelta.y * 0.01f, app->camera->Right);
        }
        else
        {
            pivot = pivot * glm::angleAxis(app->input.mouseDelta.y * 0.01f, app->camera->Right);
        }

        app->camera->Position = pivot * (app->camera->Position - reference) + reference;
        app->camera->Front = glm::normalize(reference - app->camera->Position);
        app->camera->Right = glm::normalize(glm::cross(app->camera->Front, app->camera->WorldUp));
        app->camera->Up = glm::normalize(glm::cross(app->camera->Right, app->camera->Front));

        app->camera->Pitch = glm::degrees(asin(app->camera->Front.y));
        if (app->camera->Front.z < 0.0f)
            app->camera->Yaw = -glm::degrees(acos(app->camera->Front.x / cos(glm::radians(app->camera->Pitch))));
        else
            app->camera->Yaw = glm::degrees(acos(app->camera->Front.x / cos(glm::radians(app->camera->Pitch))));

        if (app->camera->Pitch > 89.0f)
            app->camera->Pitch = 89.0f;
        if (app->camera->Pitch < -89.0f)
            app->camera->Pitch = -89.0f;
    }
}
void Update(App* app)
{
    processInput(app);
   



    MapBuffer(app->cbuffer, GL_WRITE_ONLY);
    app->globalParamsOffsetSecond = app->cbufferSecond.head;

    
    PushVec3(app->cbuffer, app->camera->Position);
    glm::mat4 matrix = glm::inverse(app->camera->projection);
    PushMat4(app->cbuffer, app->camera->projection);
    PushMat4(app->cbuffer, matrix);
    PushUInt(app->cbuffer, app->lights.size());
    for (u32 i = 0; i < app->lights.size(); i++) {
        AlignHead(app->cbuffer, sizeof(vec4));
        Light* light = app->lights[i];
        PushUInt(app->cbuffer, light->type);
        PushVec3(app->cbuffer, light->color);
        PushVec3(app->cbuffer, light->direction);
        PushVec3(app->cbuffer, light->position);
        PushUFloat(app->cbuffer, light->intensity);
        PushUFloat(app->cbuffer, light->angle);
    }
    
    app->globalParamsSize = app->cbuffer.head - app->globalParamsOffset;
    UnmapBuffer(app->cbuffer);

    ////////////////////////////////////////////////////////////////////////////////

    MapBuffer(app->cbufferSecond, GL_WRITE_ONLY);
    app->globalParamsOffsetSecond = app->cbufferSecond.head;
    
    PushUFloat(app->cbufferSecond, -1);//left
    PushUFloat(app->cbufferSecond, 1);//right
    PushUFloat(app->cbufferSecond, -1);//bottom
    PushUFloat(app->cbufferSecond, 1);//top

    PushUFloat(app->cbufferSecond, app->camera->nearP);
    PushUFloat(app->cbufferSecond, app->camera->farP);
    for (u32 i = 0; i < app->ssaoKernel.size(); i++) {
        AlignHead(app->cbufferSecond, sizeof(vec4));
        PushVec3(app->cbufferSecond, app->ssaoKernel[i]);
    }
    app->globalParamsSizeSecond = app->cbufferSecond.head - app->globalParamsOffsetSecond;
    UnmapBuffer(app->cbufferSecond);

}
GLuint FindVAO( Mesh mesh, int submeshIndex, Program program) {
    Submesh& submesh = mesh.submeshes[submeshIndex];
    for (u32 i = 0; i < (u32)submesh.vaos.size(); ++i) {
        if (submesh.vaos[i].programHandle == program.handle) {
            return submesh.vaos[i].handle;
        }
    }
    GLuint vaoHandle = 0;

    glGenVertexArrays(1, &vaoHandle);
    
    
    glBindVertexArray(vaoHandle);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vertexBufferHandle);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indexBufferHandle);

    for (u32 i = 0; i < program.vertexInputLayout.attributes.size(); ++i) {
        bool attributeWasLinked = false;
        for (u32 j = 0; j < submesh.vertexBufferLayout.attributes.size(); ++j) {
            if (program.vertexInputLayout.attributes[i].location == submesh.vertexBufferLayout.attributes[j].location) {
                const u32 index = submesh.vertexBufferLayout.attributes[j].location;
               
                const u32 ncomp = submesh.vertexBufferLayout.attributes[j].componenetCount;
                const u32 offset = submesh.vertexBufferLayout.attributes[j].offset+submesh.vertexOffset;
                const u32 stride = submesh.vertexBufferLayout.stride;
                glVertexAttribPointer(index, ncomp, GL_FLOAT, GL_FALSE, stride, (void*)(u64)offset);
                glEnableVertexAttribArray(index);
                attributeWasLinked = true;
                break;
            }
        }
        assert(attributeWasLinked);
    }
    
    glBindVertexArray(0);






    Vao vao = { vaoHandle,program.handle };
    //submesh.vaos.push_back(vao);
    return vaoHandle;
}
void Render(App* app)
{
    switch (app->mode)
    {
        case Mode_TexturedQuad:
            {
                glClearColor(0.1, 0.1, 0.1, 1.0);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                glBindFramebuffer(GL_FRAMEBUFFER, app->gBuffer);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                glBindBufferRange(GL_UNIFORM_BUFFER, BINDING(0), app->cbuffer.handle, app->globalParamsOffset, app->globalParamsSize);
                glBindBufferRange(GL_UNIFORM_BUFFER, BINDING(1), app->cbufferSecond.handle, app->globalParamsOffsetSecond, app->globalParamsSizeSecond);
    
                glViewport(0, 0, app->displaySize.x, app->displaySize.y);
                
                for (int a = 0; a < app->sceneObjects.size(); a++) 
                {
                    Program& texturedMeshPRogram = app->programs[app->sceneObjects[a]->shaderID];
                    glUseProgram(texturedMeshPRogram.handle);

                    glUniformMatrix4fv(glGetUniformLocation(texturedMeshPRogram.handle, "view"), 1, GL_FALSE, &app->camera->GetViewMatrix()[0][0]);
                    glUniformMatrix4fv(glGetUniformLocation(texturedMeshPRogram.handle, "projection"), 1, GL_FALSE, &app->camera->projection[0][0]);
                    
                    glUniform1i(glGetUniformLocation(texturedMeshPRogram.handle, "lightAffected"), app->sceneObjects[a]->showInGeneralList);
                    if (!app->sceneObjects[a]->showInGeneralList&&app->lights.size()>0) 
                    {
                        glUniform3fv(glGetUniformLocation(texturedMeshPRogram.handle, "ColorToPass"), 1, &app->sceneObjects[a]->lightAttached->color.x);
                    }

                    glUniformMatrix4fv(glGetUniformLocation(texturedMeshPRogram.handle, "model"), 1, GL_FALSE, &app->sceneObjects[a]->modelMat[0][0]);


                    Model& model = app->models[app->sceneObjects[a]->meshID];
                    Mesh& mesh = app->meshes[model.meshIdx];
                    for (u32 i = 0; i < mesh.submeshes.size(); ++i)
                    {
                        GLuint vao = FindVAO(mesh, i, texturedMeshPRogram);
                        glBindVertexArray(vao);
                        u32 submeshMaterialIdx = model.materialIdx[i];
                        Material& submeshMaterial = app->materials[submeshMaterialIdx];
                        glActiveTexture(GL_TEXTURE0);
                        if (app->textures.size() > 0) {

                            glBindTexture(GL_TEXTURE_2D, app->textures[submeshMaterial.albedoTextureIdx].handle);
                        }

             
                        Submesh& submesh = mesh.submeshes[i];
                        glDrawElements(GL_TRIANGLES, submesh.indices.size(), GL_UNSIGNED_INT, (void*)(u64)submesh.indexOffset);

                        if (app->textures.size() > 0) {
                            glBindTexture(GL_TEXTURE_2D, 0);
                        }
                        glBindVertexArray(0);
                        glDeleteVertexArrays(1, &vao);
                    }
                    
                
                
                }
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                /// //////////////////////////////////////////////////////////////////////
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                Program& texturedQuadPRogram = app->programs[app->texturedQuadProgramIdx];

                glUseProgram(texturedQuadPRogram.handle);
                glUniform1i(glGetUniformLocation(texturedQuadPRogram.handle, "FinalRenderID"), app->selectedFrameBuffer);

                //
                      
                


                //
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, app->gPosition);
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, app->gNormal);
                glActiveTexture(GL_TEXTURE2);
                glBindTexture(GL_TEXTURE_2D, app->gAlbedoSpec);
                glActiveTexture(GL_TEXTURE3);
                glBindTexture(GL_TEXTURE_2D, app->gDepth);
                glActiveTexture(GL_TEXTURE4);
                glBindTexture(GL_TEXTURE_2D, app->ggPosition);
                glActiveTexture(GL_TEXTURE5);
                glBindTexture(GL_TEXTURE_2D, app->ggNormal);
                
                glBindVertexArray(app->VAO);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                glBindVertexArray(0);

                /*glBindFramebuffer(GL_READ_FRAMEBUFFER, app->gBuffer);
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);*/ // write to default framebuffer
                
                glBlitFramebuffer(0, 0, app->displaySize.x, app->displaySize.y, 0, 0, app->displaySize.x, app->displaySize.y, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
                glBindFramebuffer(GL_FRAMEBUFFER, 0);

                //
                glBindTexture(GL_TEXTURE2, 0);
                glBindTexture(GL_TEXTURE2, 0);
                glBindTexture(GL_TEXTURE2, 0);
                glBindTexture(GL_TEXTURE2, 0);
                glBindTexture(GL_TEXTURE2, 0);
                glBindTexture(GL_TEXTURE2, 0);
                glUnmapBuffer(GL_UNIFORM_BUFFER);
                glBindBuffer(GL_UNIFORM_BUFFER, 0);
                
            }
            break;

        default:;
    }
}


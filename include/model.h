#ifndef MODEL_H
#define MODEL_H

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <fstream>
#include <algorithm>

#include "triangle.h"
#include "material.h"
#include "texture.h"


inline std::string resolveTexturePath(const std::string& rawPath, const std::string& modelDir)
{
    std::string filename = rawPath;
    size_t slash = filename.find_last_of("/\\");
    if (slash != std::string::npos) {
        filename = filename.substr(slash + 1);
    }

    std::vector<std::string> candidates = {
        modelDir + "/" + filename,
        modelDir + "/../textures/" + filename,
    };

    for (const auto& candidate : candidates) {
        std::ifstream f(candidate);
        if (f.good()) {
            return candidate;
        }
    }

    return candidates[0];
}

inline glm::mat4 aiMatrixToGLM(const aiMatrix4x4& m)
{
    return glm::mat4(
        m.a1, m.b1, m.c1, m.d1,
        m.a2, m.b2, m.c2, m.d2,
        m.a3, m.b3, m.c3, m.d3,
        m.a4, m.b4, m.c4, m.d4
    );
}

inline int getMaterial(const aiScene* scene, unsigned int aiMatIndex, const std::string& dir,
                        std::vector<Texture>& textures, std::vector<Material>& materials,
                        std::unordered_map<unsigned int, int>& materialCache)
{
    auto cached = materialCache.find(aiMatIndex);
    if (cached != materialCache.end()) {
        return cached->second;
    }

    const aiMaterial* aiMat = scene->mMaterials[aiMatIndex];

    aiString texPath;
    if (aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS){
        std::string myPath = resolveTexturePath(texPath.C_Str(), dir);
        textures.push_back(Texture::Image(myPath.c_str()));
        materials.push_back(Material::Lambertian(static_cast<int>(textures.size()) - 1));
    }
    else {
        aiColor4D color(0.5f, 0.5f, 0.5f, 1.0f);
        aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, color);
        textures.push_back(Texture::Solid(glm::vec3(color.r, color.g, color.b)));
        materials.push_back(Material::Lambertian(static_cast<int>(textures.size()) - 1));
    }

    int curIdx = static_cast<int>(materials.size()) - 1;
    materialCache[aiMatIndex] = curIdx;
    return curIdx;
}

inline void processMesh(const aiMesh* mesh, const aiScene* scene, const glm::mat4& transform,
                        const std::string& dir, std::vector<Texture>& textures,
                        std::vector<Material>& materials, std::unordered_map<unsigned int, int>& materialCache,
                        std::vector<Triangle>& triangles, const std::vector<std::string>& skipMaterials)
{
    std::string matName = scene->mMaterials[mesh->mMaterialIndex]->GetName().C_Str();
    if (std::find(skipMaterials.begin(), skipMaterials.end(), matName) != skipMaterials.end()) {
        return;
    }

    int matIndex = getMaterial(scene, mesh->mMaterialIndex, dir, textures, materials, materialCache);

    glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(transform)));

    for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
        const aiFace& face = mesh->mFaces[f];
        if (face.mNumIndices != 3) {
            continue;
        }

        glm::vec3 vertices[3];
        glm::vec3 normals[3];
        glm::vec2 uvs[3];

        for (int i = 0; i < 3; ++i) {
            unsigned int idx = face.mIndices[i];

            aiVector3D vert = mesh->mVertices[idx];
            glm::vec4 worldPos = transform * glm::vec4(vert.x, vert.y, vert.z, 1.0f);
            vertices[i] = glm::vec3(worldPos);

            if (mesh->HasNormals()) {
                aiVector3D norm = mesh->mNormals[idx];
                normals[i] = glm::normalize(normalMatrix * glm::vec3(norm.x, norm.y, norm.z));
            }
            else {
                normals[i] = glm::vec3(0.0f, 1.0f, 0.0f);
            }

            if (mesh->mTextureCoords[0]) {
                aiVector3D uv = mesh->mTextureCoords[0][idx];
                uvs[i] = glm::vec2(uv.x, uv.y);
            }
            else {
                uvs[i] = glm::vec2(0.0f);
            }
        }

        triangles.push_back(Triangle(vertices[0], vertices[1], vertices[2], matIndex,
                                    normals[0], normals[1], normals[2],
                                    uvs[0], uvs[1], uvs[2]));
    }
}

inline void processNode(const aiNode* node, const aiScene* scene, const glm::mat4& pTransform,
                        const std::string& dir, std::vector<Texture>& textures, std::vector<Material>& materials,
                        std::unordered_map<unsigned int, int>& materialCache, std::vector<Triangle>& triangles,
                        const std::vector<std::string>& skipMaterials)
{
    glm::mat4 nodeTransform = pTransform * aiMatrixToGLM(node->mTransformation);

    for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
        const aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        processMesh(mesh, scene, nodeTransform, dir, textures, materials, materialCache, triangles, skipMaterials);
    }

    for (unsigned int i = 0; i < node->mNumChildren; ++i) {
        processNode(node->mChildren[i], scene, nodeTransform, dir, textures, materials, materialCache, triangles, skipMaterials);
    }
}


inline bool loadModel(const std::string& path, std::vector<Triangle>& triangles, std::vector<Texture>& textures,
                        std::vector<Material>& materials, const glm::mat4& transform = glm::mat4(1.0f),
                        const std::vector<std::string>& skipMaterials = {})
{
    Assimp::Importer importer;

    // aiProcess_GlobalScale reads the file's own unit-scale metadata (FBX/Blender exports are
    // commonly in centimeters) and normalizes it, so the model doesn't come in 100x too big.
    const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_JoinIdenticalVertices
                                                    | aiProcess_GenSmoothNormals | aiProcess_GlobalScale);

    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode)
    {
        std::cout << "ERROR::ASSIMP:: " << importer.GetErrorString() << "\n";
        return false;
    }

    std::string dir = path.substr(0, path.find_last_of("/\\"));

    std::unordered_map<unsigned int, int> materialCache;
    processNode(scene->mRootNode, scene, transform, dir, textures, materials, materialCache, triangles, skipMaterials);

    return true;
}

#endif
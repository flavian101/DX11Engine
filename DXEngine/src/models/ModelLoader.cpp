#include "dxpch.h"
#include "ModelLoader.h"
#include "utils/Mesh/Utils/VertexAttribute.h"
#include "models/Model.h"
#include "utils/Mesh/Mesh.h"
#include "utils/Mesh/Resource/MeshResource.h"
#include "utils/Mesh/Resource/SkinnedMeshResource.h"
#include "utils/material/Material.h"
#include "utils/Texture.h"
#include <chrono>
#include <algorithm>


namespace DXEngine
{

    class AssimpImporter
    {
    public:
        Assimp::Importer importer;
    };

    ModelLoader::ModelLoader()
        :m_Importer(new AssimpImporter())
    {
        m_Importer->importer.SetPropertyInteger(AI_CONFIG_PP_LBW_MAX_WEIGHTS, 4);
    }

    ModelLoader::~ModelLoader()
    {
        delete m_Importer;
    }

    std::shared_ptr<Model> ModelLoader::LoadModel(const std::string& filepath, const ModelLoadOptions& options)
    {
        auto start = std::chrono::high_resolution_clock::now();
        ClearError();
        m_LastStats = LoadStatistics();

        //check cachefirst
        if (m_CachingEnabled)
        {
            std::string cacheKey = GenerateCacheKey(filepath, options);
            std::lock_guard<std::mutex> lock(m_CacheMutex);

            auto it = m_ModelCache.find(cacheKey);
            if (it != m_ModelCache.end())
            {
                if (auto cached = it->second.lock())
                {
                    return cached;
                }
                else
                {
                    //remove the expired weak_ptr
                    m_ModelCache.erase(it);
                }
            }
        }

        //validate File
        if (!ModelLoaderUtils::FileExists(filepath))
        {
            SetError("file does not exists: " + filepath);
            return nullptr;
        }

        // Set up Assimp post-processing flags
        unsigned int flags = aiProcess_ValidateDataStructure;

        if (options.makeLeftHanded)
            flags |= aiProcess_ConvertToLeftHanded;
        if (options.triangulate)
            flags |= aiProcess_Triangulate;
        if (options.generateNormals)
            flags |= aiProcess_GenSmoothNormals;
        if (options.generateTangents)
            flags |= aiProcess_CalcTangentSpace;
        if (options.flipUVs)
            flags |= aiProcess_FlipUVs;
        if (options.optimizeMeshes)
            flags |= aiProcess_OptimizeMeshes | aiProcess_OptimizeGraph;
        if (options.joinIdenticalVertices)
            flags |= aiProcess_JoinIdenticalVertices;
        if (options.removeRedundantMaterials)
            flags |= aiProcess_RemoveRedundantMaterials;
        if (options.fixInfacingNormals)
            flags |= aiProcess_FixInfacingNormals;
        if (options.limitBoneWeights)
            flags |= aiProcess_LimitBoneWeights;

        //load Scene
        const aiScene* scene = m_Importer->importer.ReadFile(filepath, flags);

        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
        {
            SetError("Assimp error: " + std::string(m_Importer->importer.GetErrorString()));
            return nullptr;
        }

        //store context for prcessing
        m_CurrentScene = scene;
        m_CurrentDirectory = ModelLoaderUtils::GetDirectory(filepath);

        //process the scene
        auto model = ProcessScene(scene, m_CurrentDirectory, options);

        if (model)
        {
            //post-processing
            PostProcessModel(model, options);

            //cache result
            if (m_CachingEnabled)
            {
                std::string cacheKey = GenerateCacheKey(filepath, options);
                std::lock_guard<std::mutex> lock(m_CacheMutex);
                m_ModelCache[cacheKey] = model;
            }
        }
        //load time 
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        m_LastStats.loadTimeSeconds = duration.count() / 1000.0f;

        return model;
    }

    std::shared_ptr<Model> ModelLoader::LoadModelAsync(const std::string& filepath, const ModelLoadOptions& options)
    {
        return std::shared_ptr<Model>();
    }

    std::shared_ptr<MeshResource> ModelLoader::LoadMeshResource(const std::string& filepath, const ModelLoadOptions& options)
    {
        return std::shared_ptr<MeshResource>();
    }

    std::vector<std::shared_ptr<Texture>> ModelLoader::LoadTextures(const std::string& filepath)
    {
        return std::vector<std::shared_ptr<Texture>>();
    }

    std::vector<LoadedAnimation> ModelLoader::LoadAnimations(const std::string& filepath)
    {
        return std::vector<LoadedAnimation>();
    }

    std::vector<std::shared_ptr<Model>> ModelLoader::LoadModels(const std::vector<std::string>& filepath, const ModelLoadOptions& options)
    {
        return std::vector<std::shared_ptr<Model>>();
    }

    bool ModelLoader::ValidateFile(const std::string& filePath)
    {
        if (!ModelLoaderUtils::FileExists(filePath))
        {
            SetError("File does not exist: " + filePath);
            return false;
        }

        std::string extension = ModelLoaderUtils::GetFileExtension(filePath);
        if (!ModelLoaderUtils::IsSupportedFormat(extension))
        {
            SetError("Unsupported file format: " + extension);
            return false;
        }

        return true;
    }

    std::shared_ptr<Model> ModelLoader::ProcessScene(const aiScene* scene, const std::string& directory, const ModelLoadOptions& options)
    {
        auto model = std::make_shared<Model>();

        //process root node Recursively
        ProcessNode(model, scene->mRootNode, scene, directory, options);

        if (options.globalScale != 1.0f)
        {
            DirectX::XMFLOAT3 scale(options.globalScale, options.globalScale, options.globalScale);
            model->SetScale(scale);
        }
        return model;
    }

    void ModelLoader::ProcessNode(std::shared_ptr<Model> model, const aiNode* node, const aiScene* scene, const std::string& directory, const ModelLoadOptions& options)
    {
        //process all meshes in this node 
        for (unsigned int i = 0; i < node->mNumMeshes; i++)
        {
            const aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            std::shared_ptr<MeshResource> meshResource;

            //check of mesh has bones(skinned mesh)
            if (mesh->HasBones() && options.loadAnimations)
            {
                meshResource = ProcessSkinnedMesh(mesh, scene, options);
            }
            else
            {
                meshResource = ProcessMesh(mesh, scene, options);
            }

            if (meshResource)
            {
                auto meshObject = std::make_shared<Mesh>(meshResource);

                //process Material
                if (options.loadMaterials && mesh->mMaterialIndex >= 0)
                {
                    auto material = ProcessMaterial(scene->mMaterials[mesh->mMaterialIndex], directory, options);
                    if (material)
                    {
                        meshObject->SetMaterial(material);
                    }
                }

                //add mesh To model
                std::string meshName = mesh->mName.C_Str();
                if (meshName.empty())
                    meshName = "Mesh_" + std::to_string(model->GetMeshCount());


                model->AddMesh(meshObject, meshName);
                m_LastStats.meshesLoaded++;
            }
        }
        //process child nodes recursively
        for (unsigned int i = 0; i < node->mNumChildren; i++)
        {
            ProcessNode(model, node->mChildren[i], scene, directory, options);
        }
    }

    std::shared_ptr<MeshResource> ModelLoader::ProcessMesh(const aiMesh* mesh, const aiScene* scene, const ModelLoadOptions& options)
    {
        VertexLayout layout;

        layout.Position();
        if (mesh->HasNormals() || options.generateNormals)
        {
            layout.Normal();
        }
        // Texture coordinates
        if (mesh->HasTextureCoords(0))
        {
            layout.TexCoord(0); // Uses your existing TexCoord(index) method
        }

        // Tangents (if present or generated)
        if (mesh->HasTangentsAndBitangents() || options.generateTangents)
        {
            layout.Tangent(); // Uses your existing Tangent() method
        }

        // Additional texture coordinate sets
        for (unsigned int i = 1; i < mesh->GetNumUVChannels() && i < 4; i++)
        {
            layout.TexCoord(i); // Uses your existing TexCoord(index) method
        }

        layout.Finalize();

        // Create vertex data
        auto vertexData = std::make_unique<VertexData>(layout);
        vertexData->Resize(mesh->mNumVertices);

        // Fill vertex data using your existing SetAttribute method
        for (unsigned int i = 0; i < mesh->mNumVertices; i++)
        {
            // Position
            if (mesh->HasPositions())
            {
                DirectX::XMFLOAT3 pos(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
                vertexData->SetAttribute(i, VertexAttributeType::Position, pos);
            }

            // Normal
            if (mesh->HasNormals())
            {
                DirectX::XMFLOAT3 normal(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
                vertexData->SetAttribute(i, VertexAttributeType::Normal, normal);
            }

            // Texture coordinates
            if (mesh->HasTextureCoords(0))
            {
                DirectX::XMFLOAT2 uv(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
                vertexData->SetAttribute(i, VertexAttributeType::TexCoord0, uv);
            }

            // Tangents
            if (mesh->HasTangentsAndBitangents())
            {
                DirectX::XMFLOAT4 tangent(
                    mesh->mTangents[i].x,
                    mesh->mTangents[i].y,
                    mesh->mTangents[i].z,
                    1.0f // Handedness - calculate properly in real implementation
                );
                vertexData->SetAttribute(i, VertexAttributeType::Tangent, tangent);
            }

            // Additional UV sets
            for (unsigned int uvSet = 1; uvSet < mesh->GetNumUVChannels() && uvSet < 4; uvSet++)
            {
                if (mesh->HasTextureCoords(uvSet))
                {
                    DirectX::XMFLOAT2 uv(mesh->mTextureCoords[uvSet][i].x, mesh->mTextureCoords[uvSet][i].y);
                    VertexAttributeType uvType = static_cast<VertexAttributeType>(
                        static_cast<int>(VertexAttributeType::TexCoord0) + uvSet);
                    vertexData->SetAttribute(i, uvType, uv);
                }
            }
        }

        // Create index data
        auto indexData = std::make_unique<IndexData>(
            mesh->mNumVertices > 65535 ? IndexType::UInt32 : IndexType::UInt16);

        for (unsigned int i = 0; i < mesh->mNumFaces; i++)
        {
            const aiFace& face = mesh->mFaces[i];
            if (face.mNumIndices == 3) // Only process triangles
            {
                indexData->AddTriangle(face.mIndices[0], face.mIndices[1], face.mIndices[2]);
            }
        }

        // Create mesh resource
        std::string meshName = mesh->mName.C_Str();
        if (meshName.empty())
            meshName = "LoadedMesh";

        auto meshResource = std::make_unique<MeshResource>(meshName);
        meshResource->SetVertexData(std::move(vertexData));
        meshResource->SetIndexData(std::move(indexData));
        meshResource->SetTopology(PrimitiveTopology::TriangleList);

        // Generate missing data if requested
        if (options.generateNormals && !mesh->HasNormals())
        {
            meshResource->GenerateNormals();
        }

        if (options.generateTangents && !mesh->HasTangentsAndBitangents())
        {
            meshResource->GenerateTangents();
        }

        // Generate bounds
        meshResource->GenerateBounds();

        return meshResource;
    }

    std::shared_ptr<Material> ModelLoader::ProcessMaterial(const aiMaterial* material, const std::string& directory, const ModelLoadOptions& options)
    {
        // Initialize material type - we'll determine the final type based on available textures
        MaterialType materialType = MaterialType::Lit;
        bool hasTextures = false;
        bool hasNormalMap = false;
        bool hasSpecularMap = false;
        bool hasEmissiveMap = false;
        bool isTransparent = false;
        bool isEmissive = false;

        // Check for transparency first
        float opacity = 1.0f;
        if (material->Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS && opacity < 1.0f)
        {
            isTransparent = true;
        }

        // Check for emissive properties
        aiColor3D emissive(0.0f, 0.0f, 0.0f);
        if (material->Get(AI_MATKEY_COLOR_EMISSIVE, emissive) == AI_SUCCESS)
        {
            if (emissive.r > 0.0f || emissive.g > 0.0f || emissive.b > 0.0f)
            {
                isEmissive = true;
            }
        }

        // Get material name
        aiString materialName;
        material->Get(AI_MATKEY_NAME, materialName);
        std::string matName = materialName.C_Str();
        if (matName.empty())
        {
            matName = "Material_" + std::to_string(m_LastStats.materialsLoaded);
        }

        // Create material with initial type (we'll update this later)
        auto mat = std::make_shared<Material>(matName, materialType);

        // Set basic color properties
        aiColor3D diffuse(1.0f, 1.0f, 1.0f);
        aiColor3D specular(1.0f, 1.0f, 1.0f);
        aiColor3D ambient(0.2f, 0.2f, 0.2f);
        float shininess = 32.0f;
        float metallic = 0.0f;
        float roughness = 0.5f;

        // Get diffuse color
        if (material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse) == AI_SUCCESS)
        {
            mat->SetDiffuseColor({ diffuse.r, diffuse.g, diffuse.b, opacity });
        }

        // Get specular color
        if (material->Get(AI_MATKEY_COLOR_SPECULAR, specular) == AI_SUCCESS)
        {
            mat->SetSpecularColor({ specular.r, specular.g, specular.b, 1.0f });
        }

        // Get shininess
        if (material->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS)
        {
            // Clamp shininess to reasonable range
            shininess = std::max(1.0f, std::min(shininess, 128.0f));
            mat->SetShininess(shininess);
        }

        // Set emissive color if present
        if (isEmissive)
        {
            mat->SetEmissiveColor({ emissive.r, emissive.g, emissive.b, 1.0f });
        }

        // Set alpha/transparency
        if (isTransparent)
        {
            mat->SetAlpha(opacity);
            mat->SetFlag(MaterialFlags::IsTransparent, true);
        }

        // Load textures if enabled
        if (options.loadTextures)
        {
            // Diffuse/Albedo texture
            std::string diffusePath = GetTextureFilename(material, aiTextureType_DIFFUSE, directory);
            if (!diffusePath.empty())
            {
                auto diffuseTexture = LoadTexture(diffusePath, aiTextureType_DIFFUSE);
                if (diffuseTexture && diffuseTexture->IsValid())
                {
                    mat->SetDiffuseTexture(diffuseTexture);
                    hasTextures = true;
                    OutputDebugStringA(("Loaded diffuse texture: " + diffusePath + "\n").c_str());
                }
                else
                {
                    OutputDebugStringA(("Failed to load diffuse texture: " + diffusePath + "\n").c_str());
                }
            }

            // Normal map
            std::string normalPath = GetTextureFilename(material, aiTextureType_NORMALS, directory);
            if (normalPath.empty())
            {
                // Try alternative normal map types
                normalPath = GetTextureFilename(material, aiTextureType_HEIGHT, directory);
            }
            if (!normalPath.empty())
            {
                auto normalTexture = LoadTexture(normalPath, aiTextureType_NORMALS);
                if (normalTexture && normalTexture->IsValid())
                {
                    mat->SetNormalTexture(normalTexture);
                    hasNormalMap = true;
                    OutputDebugStringA(("Loaded normal texture: " + normalPath + "\n").c_str());
                }
                else
                {
                    OutputDebugStringA(("Failed to load normal texture: " + normalPath + "\n").c_str());
                }
            }

            // Specular map
            std::string specularPath = GetTextureFilename(material, aiTextureType_SPECULAR, directory);
            if (!specularPath.empty())
            {
                auto specularTexture = LoadTexture(specularPath, aiTextureType_SPECULAR);
                if (specularTexture && specularTexture->IsValid())
                {
                    mat->SetSpecularTexture(specularTexture);
                    hasSpecularMap = true;
                    OutputDebugStringA(("Loaded specular texture: " + specularPath + "\n").c_str());
                }
                else
                {
                    OutputDebugStringA(("Failed to load specular texture: " + specularPath + "\n").c_str());
                }
            }

            // Emissive map
            std::string emissivePath = GetTextureFilename(material, aiTextureType_EMISSIVE, directory);
            if (!emissivePath.empty())
            {
                auto emissiveTexture = LoadTexture(emissivePath, aiTextureType_EMISSIVE);
                if (emissiveTexture && emissiveTexture->IsValid())
                {
                    mat->SetEmissiveTexture(emissiveTexture);
                    hasEmissiveMap = true;
                    OutputDebugStringA(("Loaded emissive texture: " + emissivePath + "\n").c_str());
                }
                else
                {
                    OutputDebugStringA(("Failed to load emissive texture: " + emissivePath + "\n").c_str());
                }
            }

            // Try to load other common texture types
            // Roughness/Metallic maps (for PBR workflows)
            std::string roughnessPath = GetTextureFilename(material, aiTextureType_DIFFUSE_ROUGHNESS, directory);
            std::string metallicPath = GetTextureFilename(material, aiTextureType_METALNESS, directory);

            // Ambient Occlusion
            std::string aoPath = GetTextureFilename(material, aiTextureType_AMBIENT_OCCLUSION, directory);

            // Note: These would require additional texture slots and material properties
            // For now, we'll just log if they're found
            if (!roughnessPath.empty())
            {
                OutputDebugStringA(("Found roughness map: " + roughnessPath + " (not loaded - requires PBR material support)\n").c_str());
            }
            if (!metallicPath.empty())
            {
                OutputDebugStringA(("Found metallic map: " + metallicPath + " (not loaded - requires PBR material support)\n").c_str());
            }
            if (!aoPath.empty())
            {
                OutputDebugStringA(("Found AO map: " + aoPath + " (not loaded - requires additional texture slot)\n").c_str());
            }
        }

        // Determine final material type based on loaded textures and properties
        if (isTransparent)
        {
            materialType = MaterialType::Transparent;
        }
        else if (hasEmissiveMap || isEmissive)
        {
            materialType = MaterialType::Emissive;
        }
        else if (hasNormalMap || hasTextures)
        {
            materialType = MaterialType::Lit;
        }
        else
        {
            materialType = MaterialType::Lit;
        }

        // Set the final material type
        mat->SetType(materialType);

        // Set additional material flags based on properties
        mat->SetFlag(MaterialFlags::castsShadows, !isTransparent && !isEmissive);
        mat->SetFlag(MaterialFlags::ReceivesShadows, !isEmissive);

        // Check for two-sided materials
        int twoSided = 0;
        if (material->Get(AI_MATKEY_TWOSIDED, twoSided) == AI_SUCCESS && twoSided != 0)
        {
            mat->SetFlag(MaterialFlags::IsTwoSided, true);
        }

        // Get texture tiling and offset if available
        aiUVTransform uvTransform;
        if (material->Get(AI_MATKEY_UVTRANSFORM(aiTextureType_DIFFUSE, 0), uvTransform) == AI_SUCCESS)
        {
            mat->SetTextureScale({ uvTransform.mScaling.x, uvTransform.mScaling.y });
            mat->SetTextureOffset({ uvTransform.mTranslation.x, uvTransform.mTranslation.y });

            OutputDebugStringA(("Material UV transform - Scale: (" +
                std::to_string(uvTransform.mScaling.x) + ", " +
                std::to_string(uvTransform.mScaling.y) + "), Offset: (" +
                std::to_string(uvTransform.mTranslation.x) + ", " +
                std::to_string(uvTransform.mTranslation.y) + ")\n").c_str());
        }

        // Log material creation summary
        OutputDebugStringA(("Created material: " + matName +
            " (Type: " + std::to_string(static_cast<int>(materialType)) +
            ", Textures: " + (hasTextures ? "Yes" : "No") +
            ", Normal: " + (hasNormalMap ? "Yes" : "No") +
            ", Transparent: " + (isTransparent ? "Yes" : "No") + ")\n").c_str());

        m_LastStats.materialsLoaded++;
        return mat;
    }

    std::shared_ptr<Texture> ModelLoader::LoadTexture(const std::string& path, aiTextureType type)
    {
        if (path.empty())
            return nullptr;

        if (m_CachingEnabled)
        {
            std::lock_guard<std::mutex> lock(m_CacheMutex);

            auto it = m_TextureCache.find(path);
            if (it != m_TextureCache.end())
            {
                if (auto cached = it->second.lock())
                {
                    return cached;
                }
                else
                {
                    m_TextureCache.erase(it);
                }
            }
        }

        //check if Texture is embedded
        if (m_CurrentScene && path[0] == '*')
        {
            return LoadEmbeddedTexture(m_CurrentScene, path);
        }

        //load from file
        std::string fullPath = ModelLoaderUtils::NormalizePath(m_CurrentDirectory + "/" + path);

        if (!ModelLoaderUtils::FileExists(fullPath))
        {
            OutputDebugStringA(("Warning: Texture file not found: " + fullPath + "\n").c_str());
            return nullptr;
        }

        try
        {
            auto texture = Texture::CreateFromFile(fullPath);

            if (texture && m_CachingEnabled)
            {
                std::lock_guard<std::mutex> lock(m_CacheMutex);
                m_TextureCache[path] = texture;
            }
            if (texture)
            {
                m_LastStats.texturesLoaded++;
            }

            return texture;
        }
        catch (const std::exception& e) {
            OutputDebugStringA(("Failed to load texture: " + std::string(e.what()) + "\n").c_str());
            return nullptr;
        }

    }

    std::shared_ptr<Texture> ModelLoader::LoadEmbeddedTexture(const aiScene* scene, const std::string& path)
    {
        int textureIndex = std::stoi(path.substr(1));

        if (textureIndex >= 0 && textureIndex < static_cast<int>(scene->mNumTextures))
        {
            const aiTexture* texture = scene->mTextures[textureIndex];

            // Handle compressed textures (PNG, JPG, etc.)
            if (texture->mHeight == 0)
            {
                // Compressed texture data
                const unsigned char* data = reinterpret_cast<const unsigned char*>(texture->pcData);
                size_t dataSize = texture->mWidth; // Width stores data size for compressed textures

                return Texture::CreateFromMemory(data, dataSize);
            }
            else
            {
                // Uncompressed texture data (RGBA)
                const aiTexel* texels = texture->pcData;
                size_t width = texture->mWidth;
                size_t height = texture->mHeight;

                // Convert aiTexel to RGBA bytes
                std::vector<unsigned char> pixels(width * height * 4);
                for (size_t i = 0; i < width * height; i++)
                {
                    pixels[i * 4 + 0] = texels[i].r;
                    pixels[i * 4 + 1] = texels[i].g;
                    pixels[i * 4 + 2] = texels[i].b;
                    pixels[i * 4 + 3] = texels[i].a;
                }

                return Texture::CreateFromPixels(pixels.data(), width, height, 4);
            }
        }

        return nullptr;
    }

    std::string ModelLoader::GetTextureFilename(const aiMaterial* material, aiTextureType type, const std::string& directory)
    {
        if (material->GetTextureCount(type) > 0)
        {
            aiString path;
            if (material->GetTexture(type, 0, &path) == AI_SUCCESS)
            {
                std::string texturePath = std::string(path.C_Str());

                // Handle embedded textures
                if (!texturePath.empty() && texturePath[0] == '*')
                    return texturePath;

                // Normalize slashes
                std::replace(texturePath.begin(), texturePath.end(), '\\', '/');

                // Strip leading ./ or ../
                while (texturePath.rfind("./", 0) == 0)
                    texturePath = texturePath.substr(2);
                while (texturePath.rfind("../", 0) == 0)
                    texturePath = texturePath.substr(3);

                return texturePath;
            }
        }
        return "";
    }

    std::shared_ptr<SkinnedMeshResource> ModelLoader::ProcessSkinnedMesh(const aiMesh* mesh, const aiScene* scene, const ModelLoadOptions& options)
    {
        //skinned vertex layout from default
        VertexLayout layout = VertexLayout::CreateSkinned();

        //create vertexData
        auto vertexData = std::make_unique<VertexData>(layout);
        vertexData->Resize(mesh->mNumVertices);

        // Fill basic vertex data (same as regular mesh)
        for (unsigned int i = 0; i < mesh->mNumVertices; i++)
        {
            if (mesh->HasPositions())
            {
                DirectX::XMFLOAT3 pos(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
                vertexData->SetAttribute(i, VertexAttributeType::Position, pos);
            }
            // Normal
            if (mesh->HasNormals())
            {
                DirectX::XMFLOAT3 normal(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
                vertexData->SetAttribute(i, VertexAttributeType::Normal, normal);
            }

            // Texture coordinates
            if (mesh->HasTextureCoords(0))
            {
                DirectX::XMFLOAT2 uv(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
                vertexData->SetAttribute(i, VertexAttributeType::TexCoord0, uv);
            }

            // Tangents
            if (mesh->HasTangentsAndBitangents())
            {
                DirectX::XMFLOAT4 tangent(
                    mesh->mTangents[i].x,
                    mesh->mTangents[i].y,
                    mesh->mTangents[i].z,
                    1.0f
                );
                vertexData->SetAttribute(i, VertexAttributeType::Tangent, tangent);
            }
        }

        //process bone weights and indices 
        if (mesh->HasBones())
        {
            //initialize bone data for all vertices
            for (unsigned int i = 0; i < mesh->mNumVertices; i++)
            {
                // Initialize with zeros - you'll need to add these as explicit template specializations
                // or handle them in your SetAttribute method
                DirectX::XMFLOAT4 weights(0.0f, 0.0f, 0.0f, 0.0f);
                uint32_t indices[4] = { 0, 0, 0, 0 }; // This might need to be handled differently based on your DataFormat

                vertexData->SetAttribute(i, VertexAttributeType::BlendWeights, weights);
                // Note: BlendIndices might need special handling based on your DataFormat (UByte4, etc.)
                // vertexData->SetAttribute(i, VertexAttributeType::BlendIndices, indices);
            }
            for (unsigned int boneIndex = 0; boneIndex < mesh->mNumBones; boneIndex++)
            {
                const aiBone* bone = mesh->mBones[boneIndex];

                for (unsigned int weightIndex = 0; weightIndex < bone->mNumWeights; weightIndex++)
                {
                    const aiVertexWeight& weight = bone->mWeights[weightIndex];
                    unsigned int vertexId = weight.mVertexId;
                    float weightValue = weight.mWeight;

                    //get current blend data

                    auto currentWeights = vertexData->GetAttribute<DirectX::XMFLOAT4>(vertexId, VertexAttributeType::BlendWeights);

                    //find empty slot and add weight/ index
                    //this is simplified- you nend to handle the case where there are more than 4 influences
                    if (currentWeights.x == 0.0f)
                    {
                        currentWeights.x = weightValue;
                        //set bone index in blend Indices- implementation depends on your formart
                    }
                    else if (currentWeights.y == 0.0f) {
                        currentWeights.y = weightValue;
                    }
                    else if (currentWeights.z == 0.0f) {
                        currentWeights.z = weightValue;
                    }
                    else if (currentWeights.w == 0.0f) {
                        currentWeights.w = weightValue;
                    }
                    vertexData->SetAttribute(vertexId, VertexAttributeType::BlendWeights, currentWeights);
                }
            }
        }

        // add index data 
        auto indexData = std::make_unique<IndexData>(
            mesh->mNumVertices > 65535 ? IndexType::UInt32 : IndexType::UInt16);

        for (unsigned int i = 0; i < mesh->mNumFaces; i++)
        {
            const aiFace& face = mesh->mFaces[i];
            if (face.mNumIndices == 3)
            {
                indexData->AddTriangle(face.mIndices[0], face.mIndices[1], face.mIndices[2]);
            }
        }

        //now that we have both vertex data with the set layout and Index data
        //we will create the mesh Resource

        std::string meshName = mesh->mName.C_Str();
        if (meshName.empty())
            meshName = "SkinnedMesh";

        auto skinnedMesh = std::make_unique<SkinnedMeshResource>(meshName);
        skinnedMesh->SetVertexData(std::move(vertexData));
        skinnedMesh->SetIndexData(std::move(indexData));
        skinnedMesh->SetTopology(PrimitiveTopology::TriangleList);

        std::vector<LoadedBone> bones = ProcessBones(mesh);
        for (const auto& bone : bones)
        {
            SkinnedMeshResource::BoneInfo boneInfo;
            boneInfo.name = bone.name;
            boneInfo.offsetMatrix = bone.offsetMatrix;
            boneInfo.parentIndex = bone.parentIndex;
            skinnedMesh->AddBone(boneInfo);
        }

        m_LastStats.bonesLoaded += static_cast<uint32_t>(bones.size());

        return skinnedMesh;
    }

    std::vector<LoadedAnimation> ModelLoader::ProcessAnimations(const aiScene* scene)
    {
        std::vector<LoadedAnimation> animations;
        animations.reserve(scene->mNumAnimations);

        for (unsigned int i = 0; i < scene->mNumAnimations; i++)
        {
            const aiAnimation* anim = scene->mAnimations[i];
            LoadedAnimation loadedAnim;
            loadedAnim.name = anim->mName.C_Str();
            loadedAnim.duration = static_cast<float>(anim->mDuration);
            loadedAnim.ticksPerSecond = static_cast<float>(anim->mTicksPerSecond);

            // Process animation channels
            // This is a simplified version - real implementation would process keyframes
            loadedAnim.boneTransforms.resize(anim->mNumChannels);

            animations.push_back(loadedAnim);
        }

        m_LastStats.animationsLoaded += static_cast<uint32_t>(animations.size());
        return animations;
    }

    std::vector<LoadedBone> ModelLoader::ProcessBones(const aiMesh* mesh)
    {
        std::vector<LoadedBone> bones;
        bones.reserve(mesh->mNumBones);

        for (unsigned int i = 0; i < mesh->mNumBones; i++)
        {
            const aiBone* bone = mesh->mBones[i];
            LoadedBone loadedBone;
            loadedBone.name = bone->mName.C_Str();
            loadedBone.offsetMatrix = ConvertMatrix(bone->mOffsetMatrix);
            loadedBone.parentIndex = -1; //to be filled latter during hierarchy processing

            bones.push_back(loadedBone);
        }
        return bones;
    }


    DirectX::XMFLOAT4X4 ModelLoader::ConvertMatrix(const aiMatrix4x4& matrix)
    {
        return DirectX::XMFLOAT4X4(
            matrix.a1, matrix.b1, matrix.c1, matrix.d1,
            matrix.a2, matrix.b2, matrix.c2, matrix.d2,
            matrix.a3, matrix.b3, matrix.c3, matrix.d3,
            matrix.a4, matrix.b4, matrix.c4, matrix.d4
        );
    }

    DirectX::XMFLOAT3 ModelLoader::ConvertVector3(const aiVector3D& vector)
    {
        return DirectX::XMFLOAT3(vector.x, vector.y, vector.z);
    }

    DirectX::XMFLOAT4 ModelLoader::ConvertColor(const aiColor4D& color)
    {
        return DirectX::XMFLOAT4(color.r, color.g, color.b, color.a);
    }

    void ModelLoader::PostProcessModel(std::shared_ptr<Model> model, const ModelLoadOptions& options)
    {
        if (options.optimizeMeshes)
        {
            OptimizeModel(model);
        }

        if (options.validateDataStructure)
        {
            ValidateModel(model);
        }

        // Ensure default materials for all meshes
        model->EnsureDefaultMaterials();

        // Calculate memory usage
        m_LastStats.memoryUsed = model->GetMemoryUsage();
    }

    void ModelLoader::OptimizeModel(std::shared_ptr<Model> model)
    {

       //for (size_t i = 0; i < model->GetMeshCount(); i++)
       //{
       //    auto mesh = model->GetMesh(i);
       //    if (mesh && mesh->IsValid())
       //    {
       //        auto meshResource = mesh->GetResource();
       //        if (meshResource)
       //        {
       //            meshResource->OptimizeForRendering();
       //        }
       //    }
       //}
    }

    void ModelLoader::ValidateModel(std::shared_ptr<Model> model)
    {
        if (!model || !model->IsValid())
        {
            SetError("Model validation failed: Invalid model");
            return;
        }

        // Validate each mesh
        for (size_t i = 0; i < model->GetMeshCount(); i++)
        {
            auto mesh = model->GetMesh(i);
            if (!mesh || !mesh->IsValid())
            {
                SetError("Model validation failed: Invalid mesh at index " + std::to_string(i));
                return;
            }

            auto meshResource = mesh->GetResource();
            if (meshResource)
            {
                std::string errorMessage;
                if (!MeshUtils::ValidateMesh(*meshResource, errorMessage))
                {
                    SetError("Mesh validation failed: " + errorMessage);
                    return;
                }
            }
        }
    }

    std::vector<std::string> ModelLoader::GetSupportedFormats() const
    {
        return {
                    "FBX (.fbx)", "Wavefront OBJ (.obj)", "Collada (.dae)",
                    "glTF (.gltf, .glb)", "3D Studio Max (.3ds)", "ASCII Scene Exporter (.ase)",
                    "Stanford PLY (.ply)", "DirectX (.x)", "Quake III BSP (.bsp)",
                    "Doom 3 MD5 (.md5mesh)", "Inter-Quake Model (.iqm)",
                    "Biovision BVH (.bvh)", "CharacterStudio Motion (.csm)"
        };
    }

    std::string ModelLoader::GetFileInfo(const std::string& filePath)
    {
        if (!ValidateFile(filePath))
        {
            return "Invalid file: " + GetLastError();
        }

        // Quick scene info without full load
        const aiScene* scene = m_Importer->importer.ReadFile(filePath,
            aiProcess_ValidateDataStructure | aiProcess_PopulateArmatureData);

        if (!scene)
        {
            return "Failed to read file: " + std::string(m_Importer->importer.GetErrorString());
        }

        std::ostringstream info;
        info << "File: " << ModelLoaderUtils::GetFileName(filePath) << "\n";
        info << "Format: " << ModelLoaderUtils::GetFileExtension(filePath) << "\n";
        info << "Meshes: " << scene->mNumMeshes << "\n";
        info << "Materials: " << scene->mNumMaterials << "\n";
        info << "Textures: " << scene->mNumTextures << "\n";
        info << "Animations: " << scene->mNumAnimations << "\n";
        info << "Cameras: " << scene->mNumCameras << "\n";
        info << "Lights: " << scene->mNumLights << "\n";

        return info.str();
    }

    void ModelLoader::ClearCache()
    {
        std::lock_guard<std::mutex> lock(m_CacheMutex);
        m_ModelCache.clear();
        m_TextureCache.clear();
        m_MaterialCache.clear();
    }
    void ModelLoader::SetError(const std::string& error)
    {
        m_LastError = error;
        OutputDebugStringA(("ModelLoader Error: " + error + "\n").c_str());

    }

    void ModelLoader::ClearError()
    {
        m_LastError.clear();
    }

    std::string ModelLoader::GenerateCacheKey(const std::string& filePath, const ModelLoadOptions& options)
    {
        // / Simple hash - based cache key generation
             // In production, you'd want a more sophisticated hash
        return filePath + "_" + std::to_string(
            options.generateNormals + options.generateTangents * 2 + options.flipUVs * 4);
    }

    namespace ModelLoaderUtils
    {
        bool FileExists(const std::string& filePath)
        {
            return std::filesystem::exists(filePath);
        }

        std::string GetDirectory(const std::string& filePath)
        {
            return std::filesystem::path(filePath).parent_path().string();
        }
        std::string GetFormatDescription(const std::string& extension)
        {
            // Add format descriptions as needed
            static std::unordered_map<std::string, std::string> formatDescriptions = {
                {".fbx", "Autodesk FBX"},
                {".obj", "Wavefront OBJ"},
                {".gltf", "GL Transmission Format"},
                {".glb", "GL Transmission Format Binary"}
                // Add more as needed
            };

            auto it = formatDescriptions.find(extension);
            return it != formatDescriptions.end() ? it->second : "Unknown Format";
        }

        std::string GetFileName(const std::string& filePath)
        {
            return std::filesystem::path(filePath).filename().string();
        }

        bool IsValidModelFile(const std::string& filePath)
        {
            if (!FileExists(filePath))
                return false;

            std::string ext = GetFileExtension(filePath);
            return IsSupportedFormat(ext);
        }

        std::string GetFileExtension(const std::string& filePath)
        {
            return std::filesystem::path(filePath).extension().string();
        }

        std::string NormalizePath(const std::string& path)
        {
            return std::filesystem::path(path).lexically_normal().string();
        }

        ModelLoadOptions GetGameOptimizedOptions()
        {
            ModelLoadOptions options;
            options.generateNormals = true;
            options.generateTangents = true;
            options.optimizeMeshes = true;
            options.joinIdenticalVertices = true;
            options.removeRedundantMaterials = true;
            options.limitBoneWeights = true;
            options.maxBoneWeights = 4;
            options.triangulate = true;
            options.loadAnimations = true;
            options.loadMaterials = true;
            options.loadTextures = true;
            return options;
        }

        ModelLoadOptions GetHighQualityOptions()
        {
            ModelLoadOptions options;
            options.generateNormals = false; // Preserve original normals
            options.generateTangents = true;
            options.optimizeMeshes = false; // Preserve original mesh structure
            options.joinIdenticalVertices = false;
            options.removeRedundantMaterials = false;
            options.limitBoneWeights = false;
            options.triangulate = true;
            options.loadAnimations = true;
            options.loadMaterials = true;
            options.loadTextures = true;
            return options;
        }

        ModelLoadOptions GetFastLoadOptions()
        {
            ModelLoadOptions options;
            options.generateNormals = false;
            options.generateTangents = false;
            options.optimizeMeshes = true;
            options.joinIdenticalVertices = true;
            options.removeRedundantMaterials = true;
            options.limitBoneWeights = true;
            options.maxBoneWeights = 2;
            options.triangulate = true;
            options.loadAnimations = false;
            options.loadMaterials = false;
            options.loadTextures = false;
            return options;
        }

        bool IsSupportedFormat(const std::string& extension)
        {
            static std::vector<std::string> supportedFormats = {
                ".fbx", ".obj", ".dae", ".gltf", ".glb", ".3ds", ".ase", ".ply",
                ".x", ".md3", ".md2", ".md5mesh", ".iqm", ".bvh", ".csm"
            };

            std::string lowerExt = extension;
            std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(), ::tolower);

            return std::find(supportedFormats.begin(), supportedFormats.end(), lowerExt) != supportedFormats.end();
        }
    }
}
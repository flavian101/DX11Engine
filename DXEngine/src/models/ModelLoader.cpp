#include "dxpch.h"
#include "ModelLoader.h"
#include "utils/Mesh/Utils/VertexAttribute.h"
#include "models/Model.h"
#include "utils/Mesh/Mesh.h"
#include "utils/Mesh/Resource/MeshResource.h"
#include "utils/Mesh/Resource/SkinnedMeshResource.h"
#include "models/SkinnedModel.h"
#include "utils/material/Material.h"
#include "utils/Texture.h"
#include <chrono>
#include <algorithm>
#include "Animation/AnimationClip.h"
#include <set>
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

    std::vector<std::shared_ptr<AnimationClip>> ModelLoader::LoadAnimations(const std::string& filepath)
    {
        std::vector<std::shared_ptr<AnimationClip>> animations;

        ClearError();

        if (!ModelLoaderUtils::FileExists(filepath))
        {
            SetError("File does not exist: " + filepath);
            return animations;
        }

        // Load the scene
        const aiScene* scene = m_Importer->importer.ReadFile(filepath,
            aiProcess_ValidateDataStructure | aiProcess_PopulateArmatureData);

        if (!scene || !scene->HasAnimations())
        {
            if (!scene)
            {
                SetError("Assimp error: " + std::string(m_Importer->importer.GetErrorString()));
            }
            else
            {
                SetError("No animations found in file");
            }
            return animations;
        }

        // We need a skeleton to process animations
        // Try to find a skeleton from the first skinned mesh
        std::shared_ptr<Skeleton> skeleton;

        for (unsigned int i = 0; i < scene->mNumMeshes && !skeleton; i++)
        {
            const aiMesh* mesh = scene->mMeshes[i];
            if (mesh->HasBones())
            {
                skeleton = ProcessSkeleton(scene, mesh);
                break;
            }
        }

        if (!skeleton)
        {
            SetError("Cannot load animations without a skeleton");
            return animations;
        }

        // Process all animations
        animations = ProcessAnimations(scene, skeleton);

        return animations;
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
        // Reset current skeleton for new model
        m_CurrentSkeleton.reset();

        bool hasAnimations = scene->HasAnimations() && options.loadAnimations;
        bool hasSkinning = false;

        // Check if any mesh has bones
        for (unsigned int i = 0; i < scene->mNumMeshes; i++)
        {
            if (scene->mMeshes[i]->HasBones())
            {
                hasSkinning = true;
                break;
            }
        }

        std::shared_ptr<Model> model;

        // Create appropriate model type
        if (hasSkinning)
        {
            auto skinnedModel = std::make_shared<SkinnedModel>();
            model = skinnedModel;

            // Process nodes (this will create skeleton via ProcessSkinnedMesh)
            ProcessNode(model, scene->mRootNode, scene, directory, options);

            // Set skeleton on the skinned model
            if (m_CurrentSkeleton)
            {
                skinnedModel->SetSkeleton(m_CurrentSkeleton);

                //Load and process Animations
                if (hasAnimations)
                {
                    auto animations = ProcessAnimations(scene, m_CurrentSkeleton);

                    //Add all animationa to the skinned model
                    for (const auto& clip : animations)
                    {
                        if (clip)
                        {
                            skinnedModel->AddAnimationClip(clip);
                        }
                    }

                    //create animation controller with first animation
                    if (!animations.empty())
                    {
                        auto controller = std::make_shared<AnimationController>(m_CurrentSkeleton);
                        controller->SetClip(animations[0]);
                        skinnedModel->SetAnimationController(controller);

                        OutputDebugStringA(("Loaded " + std::to_string(animations.size()) +
                            " animation(s) for model\n").c_str());

                        // Store all animations somewhere accessible
                  // You might want to add a method to SkinnedModel to store all clips
                    }
                }
            }
            else if(hasSkinning)
            {
                OutputDebugStringA("Warning: Model has skinning data but no skeleton was created\n");
            }  
        }
        else
        {
            model = std::make_shared<Model>();
            ProcessNode(model, scene->mRootNode, scene, directory, options);
        }

        // Apply global scale
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
        // Get material name
        aiString materialName;
        material->Get(AI_MATKEY_NAME, materialName);
        std::string matName = materialName.C_Str();
        if (matName.empty())
        {
            matName = "Material_" + std::to_string(m_LastStats.materialsLoaded);
        }

        // Determine material type based on available textures
        MaterialType materialType = DetermineMaterialType(material);
        auto mat = std::make_shared<Material>(matName, materialType);

        // Load basic material properties
        LoadBasicMaterialProperties(mat, material);

        if (options.loadTextures)
        {
            // Load all texture types systematically
            LoadAllTextures(mat, material, directory, options);
        }

        // Configure material based on loaded textures
        ConfigureMaterialFromTextures(mat);

        m_LastStats.materialsLoaded++;
        return mat;
    }

    MaterialType ModelLoader::DetermineMaterialType(const aiMaterial* material)
    {
        //check for transparency
        float opacity = 1.0f;
        if (material->Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS && opacity < 1.0f)
        {
            return MaterialType::Transparent;
        }

        // Check for emissive properties
        aiColor3D emissive(0.0f, 0.0f, 0.0f);
        if (material->Get(AI_MATKEY_COLOR_EMISSIVE, emissive) == AI_SUCCESS)
        {
            if (emissive.r > 0.1f || emissive.g > 0.1f || emissive.b > 0.1f)
            {
                return MaterialType::Emissive;
            }
        }

        // Check for PBR workflow (presence of metallic/roughness)
        if (material->GetTextureCount(aiTextureType_METALNESS) > 0 ||
            material->GetTextureCount(aiTextureType_DIFFUSE_ROUGHNESS) > 0)
        {
            return MaterialType::PBR;
        }

        // Default to lit material
        return MaterialType::Lit;
    }

    void ModelLoader::LoadBasicMaterialProperties(std::shared_ptr<Material> mat, const aiMaterial* material)
    {
        //get Basic Colors
        aiColor3D diffuse(1.0f, 1.0f, 1.0f);
        aiColor3D specular(1.0f, 1.0f, 1.0f);
        aiColor3D emissive(0.0f, 0.0f, 0.0f);
        float opacity = 1.0f;
        float shininess = 32.0f;
        float metallic = 0.0f;
        float roughness = 0.5f;

        material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);
        material->Get(AI_MATKEY_COLOR_SPECULAR, specular);
        material->Get(AI_MATKEY_COLOR_EMISSIVE, emissive);
        material->Get(AI_MATKEY_OPACITY, opacity);
        material->Get(AI_MATKEY_SHININESS, shininess);
        material->Get(AI_MATKEY_METALLIC_FACTOR, metallic);
        material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness);

        // Set material properties
        mat->SetDiffuseColor({ diffuse.r, diffuse.g, diffuse.b, opacity });
        mat->SetSpecularColor({ specular.r, specular.g, specular.b, 1.0f });
        mat->SetEmissiveColor({ emissive.r, emissive.g, emissive.b, 1.0f });
        mat->SetShininess(std::clamp(shininess, 1.0f, 128.0f));
        mat->SetAlpha(opacity);
        mat->SetMetallic(metallic);
        mat->SetRoughness(roughness);
    }

    void ModelLoader::LoadAllTextures(std::shared_ptr<Material> mat, const aiMaterial* material, const std::string& directory, const ModelLoadOptions& options)
    {
        // Core textures
        LoadTextureOfType(mat, material, directory, aiTextureType_DIFFUSE,
            [mat](auto tex) { mat->SetDiffuseTexture(tex); });
        LoadTextureOfType(mat, material, directory, aiTextureType_NORMALS,
            [mat](auto tex) { mat->SetNormalTexture(tex); });
        LoadTextureOfType(mat, material, directory, aiTextureType_SPECULAR,
            [mat](auto tex) { mat->SetSpecularTexture(tex); });
        LoadTextureOfType(mat, material, directory, aiTextureType_EMISSIVE,
            [mat](auto tex) { mat->SetEmissiveTexture(tex); });

        // PBR textures - NEW
        LoadTextureOfType(mat, material, directory, aiTextureType_METALNESS,
            [mat](auto tex) { mat->SetMetallicTexture(tex); });
        LoadTextureOfType(mat, material, directory, aiTextureType_DIFFUSE_ROUGHNESS,
            [mat](auto tex) { mat->SetRoughnessTexture(tex); });
        LoadTextureOfType(mat, material, directory, aiTextureType_AMBIENT_OCCLUSION,
            [mat](auto tex) { mat->SetAOTexture(tex); });
        LoadTextureOfType(mat, material, directory, aiTextureType_HEIGHT,
            [mat](auto tex) { mat->SetHeightTexture(tex); });
        LoadTextureOfType(mat, material, directory, aiTextureType_OPACITY,
            [mat](auto tex) { mat->SetOpacityTexture(tex); });

        if (mat->HasHeightTexture() && !IsItHeightMap(mat->GetHeightTexture())) {
            // Likely a misassigned normal map; reassign if no normal is set
            if (!mat->HasNormalTexture()) {
                mat->SetNormalTexture(mat->GetHeightTexture());
#ifdef DX_DEBUG
                OutputDebugStringA("Reassigned height texture to normal map based on filename check.\n");
#endif
            }
            mat->SetHeightTexture(nullptr);
        }
    }

    void ModelLoader::LoadTextureOfType(std::shared_ptr<Material> mat, const aiMaterial* material, const std::string& directory, aiTextureType type, std::function<void(std::shared_ptr<Texture>)> setter)
    {
        std::string texturePath = GetTextureFilename(material, type, directory);

        if (!texturePath.empty())
        {
            auto texture = LoadTexture(texturePath, type);
            if (texture && texture->IsValid())
            {
                setter(texture);
#ifdef DX_DEBUG
                OutputDebugStringA(("Loaded " + GetTextureTypeName(type) +
                    " texture: " + texturePath + "\n").c_str());
#endif
            }
            else
            {
                OutputDebugStringA(("Failed to load " + GetTextureTypeName(type) +
                    " texture: " + texturePath + "\n").c_str());
            }
        }


    }

    bool ModelLoader::IsItHeightMap(std::shared_ptr<Texture> texture)
    {
        if (!texture)
            return false;

        // Get the filename (assuming Texture has GetPath() or similar)
        const std::string& filename = texture->GetFilePath();

        // Reuse your existing detection utility
        TextureType type = TextureUtils::DetectTextureType(filename);

        return (type == TextureType::Height);

    }


    std::shared_ptr<Texture> ModelLoader::CreateFallbackTexture(aiTextureType type)
    {
        switch (type)
        {
        case aiTextureType_DIFFUSE:
            return CreateSolidColorTexture(255, 255, 255, 255); // White

        case aiTextureType_NORMALS:
            return CreateSolidColorTexture(128, 128, 255, 255); // Flat normal (0.5, 0.5, 1.0)

        case aiTextureType_SPECULAR:
            return CreateSolidColorTexture(255, 255, 255, 255); // Full specular

        case aiTextureType_METALNESS:
            return CreateSolidColorTexture(0, 0, 0, 255);       // Non-metallic

        case aiTextureType_DIFFUSE_ROUGHNESS:
            return CreateSolidColorTexture(128, 128, 128, 255); // Medium roughness

        case aiTextureType_AMBIENT_OCCLUSION:
            return CreateSolidColorTexture(255, 255, 255, 255); // No occlusion

        case aiTextureType_HEIGHT:
            return CreateSolidColorTexture(0, 0, 0, 255);       // Flat height

        case aiTextureType_EMISSIVE:
            return CreateSolidColorTexture(0, 0, 0, 255);       // No emission

        case aiTextureType_OPACITY:
            return CreateSolidColorTexture(255, 255, 255, 255); // Fully opaque

        default:
            return CreateSolidColorTexture(255, 0, 255, 255);   // Magenta for unknown
        }
    }

    std::shared_ptr<Texture> ModelLoader::CreateSolidColorTexture(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
    {
        // Create a 1x1 texture with the specified color
        uint8_t pixels[4] = { r, g, b, a };
        return Texture::CreateFromPixels(pixels, 1, 1, 4);
    }

    void ModelLoader::ConfigureMaterialFromTextures(std::shared_ptr<Material> mat)
    {
        // Update material type based on loaded textures
        if (mat->HasRoughnessTexture() || mat->HasMetallicTexture())
        {
            if (mat->GetType() == MaterialType::Lit)
            {
                mat->SetType(MaterialType::PBR);
            }
        }

        // Configure parallax mapping if height texture exists
        if (mat->HasHeightTexture())
        {
            mat->SetFlag(MaterialFlags::UseParallaxMapping, true);
            mat->SetHeightScale(0.05f);
        }

        // Configure detail textures if available
        if (mat->IsTextureSlotUsed(TextureSlot::DetailDiffuse) ||
            mat->IsTextureSlotUsed(TextureSlot::DetailNormal))
        {
            mat->SetFlag(MaterialFlags::UseDetailTextures, true);
            mat->SetDetailTextureScale({ 8.0f, 8.0f }); // Default 8x8 tiling
        }
    }

    std::string ModelLoader::GetTextureTypeName(aiTextureType type)
    {
        switch (type)
        {
        case aiTextureType_DIFFUSE: return "Diffuse";
        case aiTextureType_NORMALS: return "Normal";
        case aiTextureType_SPECULAR: return "Specular";
        case aiTextureType_EMISSIVE: return "Emissive";
        case aiTextureType_METALNESS: return "Metallic";
        case aiTextureType_DIFFUSE_ROUGHNESS: return "Roughness";
        case aiTextureType_AMBIENT_OCCLUSION: return "AO";
        case aiTextureType_HEIGHT: return "Height";
        case aiTextureType_DISPLACEMENT: return "Displacement";
        case aiTextureType_OPACITY: return "Opacity";
        case aiTextureType_REFLECTION: return "Reflection";
        default: return "Unknown";
        }
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
            //initialize bone data to Zero
            DirectX::XMFLOAT4 weights(0.0f, 0.0f, 0.0f, 0.0f);
            DirectX::XMINT4 indices(0, 0, 0, 0);
            vertexData->SetAttribute(i, VertexAttributeType::BlendWeights,weights);
            vertexData->SetAttribute(i, VertexAttributeType::BlendIndices,indices);
        }

        // Process skeleton (reuse if already created for this model)
        if (!m_CurrentSkeleton)
        {
            m_CurrentSkeleton = ProcessSkeleton(scene, mesh);
        }

        // Process bone weights and indices
        if (mesh->HasBones() && m_CurrentSkeleton)
        {
            // Temporary storage for bone influences per vertex
            struct VertexBoneData
            {
                std::vector<std::pair<int, float>> influences; // bone index, weight
            };
            std::vector<VertexBoneData> vertexBoneData(mesh->mNumVertices);

            // Collect all bone influences
            for (unsigned int boneIndex = 0; boneIndex < mesh->mNumBones; boneIndex++)
            {
                const aiBone* bone = mesh->mBones[boneIndex];
                std::string boneName = bone->mName.C_Str();

                int skeletonBoneIndex = m_CurrentSkeleton->GetBoneIndex(boneName);
                if (skeletonBoneIndex < 0)
                    continue;

                for (unsigned int weightIndex = 0; weightIndex < bone->mNumWeights; weightIndex++)
                {
                    const aiVertexWeight& weight = bone->mWeights[weightIndex];
                    unsigned int vertexId = weight.mVertexId;
                    float weightValue = weight.mWeight;

                    vertexBoneData[vertexId].influences.push_back({ skeletonBoneIndex, weightValue });
                }
            }

            // Now assign top 4 influences to each vertex
            for (unsigned int i = 0; i < mesh->mNumVertices; i++)
            {
                auto& influences = vertexBoneData[i].influences;

                // Sort by weight (descending)
                std::sort(influences.begin(), influences.end(),
                    [](const auto& a, const auto& b) { return a.second > b.second; });

                // Take top 4 and normalize
                DirectX::XMINT4 indices(0, 0, 0, 0);
                DirectX::XMFLOAT4 weights(0.0f, 0.0f, 0.0f, 0.0f);

                float totalWeight = 0.0f;
                size_t count = std::min(influences.size(), size_t(4));

                for (size_t j = 0; j < count; j++)
                {
                    reinterpret_cast<int*>(&indices)[j] = influences[j].first;
                    reinterpret_cast<float*>(&weights)[j] = influences[j].second;
                    totalWeight += influences[j].second;
                }

                // Normalize weights
                if (totalWeight > 0.0f)
                {
                    weights.x /= totalWeight;
                    weights.y /= totalWeight;
                    weights.z /= totalWeight;
                    weights.w /= totalWeight;
                }

                vertexData->SetAttribute(i, VertexAttributeType::BlendIndices, indices);
                vertexData->SetAttribute(i, VertexAttributeType::BlendWeights, weights);
            }
        }

        // Create index data
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

        // Create skinned mesh resource
        std::string meshName = mesh->mName.C_Str();
        if (meshName.empty())
            meshName = "SkinnedMesh";

        auto skinnedMesh = std::make_shared<SkinnedMeshResource>(meshName);
        skinnedMesh->SetVertexData(std::move(vertexData));
        skinnedMesh->SetIndexData(std::move(indexData));
        skinnedMesh->SetTopology(PrimitiveTopology::TriangleList);
        skinnedMesh->SetSkeleton(m_CurrentSkeleton);

        // Generate missing data if requested
        if (options.generateNormals && !mesh->HasNormals())
        {
            skinnedMesh->GenerateNormals();
        }

        if (options.generateTangents && !mesh->HasTangentsAndBitangents())
        {
            skinnedMesh->GenerateTangents();
        }

        skinnedMesh->GenerateBounds();

        return skinnedMesh;

    }

    std::shared_ptr<Skeleton> ModelLoader::ProcessSkeleton(const aiScene* scene, const aiMesh* mesh)
    {
        if (!mesh->HasBones())
            return nullptr;

        auto skeleton = std::make_shared<Skeleton>();
        std::unordered_map<std::string, int> boneMapping;

        //first pass: create boned from mesh bone data(get offset matrices)
        for (unsigned int i = 0; i < mesh->mNumBones; i++)
        {
            const aiBone* aiBone = mesh->mBones[i];
            std::string boneName = aiBone->mName.C_Str();

            Bone bone;
            bone.Name = boneName;
            bone.OffsetMatrix = ConvertMatrix(aiBone->mOffsetMatrix);
            bone.ParentIndex = -1; //to be set in hierarchy pass

            //initialize local transform to identity
            DirectX::XMStoreFloat4x4(&bone.LocalTransform, DirectX::XMMatrixIdentity());

            boneMapping[boneName] = static_cast<int>(skeleton->GetBoneCount());
            skeleton->AddBone(bone);
        }

        //second Pass: build hierarchy from scene node structure
        if (scene->mRootNode)
        {
            ProcessNodeHierarchy(scene->mRootNode, scene, skeleton, -1, boneMapping);
        }
        m_LastStats.bonesLoaded += static_cast<uint32_t>(skeleton->GetBoneCount());

        return skeleton;
    }

    void ModelLoader::ProcessNodeHierarchy(const aiNode* node, const aiScene* scene, std::shared_ptr<Skeleton> skeleton, int parentIndex, std::unordered_map<std::string, int>& boneMapping)
    {
        std::string nodeName = node->mName.C_Str();
        int currentBoneIndex = -1;

        //check if this node corresponds to a bone
        auto it = boneMapping.find(nodeName);
        if (it != boneMapping.end())
        {
            currentBoneIndex = it->second;
            Bone& bone = skeleton->GetBone(currentBoneIndex);

            //set parent index
            bone.ParentIndex = parentIndex;
            //set local Transform form node transformation
            bone.LocalTransform = ConvertMatrix(node->mTransformation);
            //use this bone as parent for Children
            parentIndex = currentBoneIndex;
        }

        for (unsigned int i = 0; i < node->mNumChildren; i++)
        {
            ProcessNodeHierarchy(node->mChildren[i], scene, skeleton, parentIndex, boneMapping);
        }
    }

    std::vector<std::shared_ptr<AnimationClip>> ModelLoader::ProcessAnimations(const aiScene* scene, std::shared_ptr<Skeleton> skeleton)
    {
        std::vector<std::shared_ptr<AnimationClip>> animations;

        if (!scene->HasAnimations() || !skeleton)
            return animations;

        animations.reserve(scene->mNumAnimations);

        for (unsigned int i = 0; i < scene->mNumAnimations; i++)
        {
            const aiAnimation* aiAnim = scene->mAnimations[i];
            auto clip = ProcessAnimation(aiAnim, skeleton);
            if (clip)
            {
                animations.push_back(clip);
                m_LastStats.animationsLoaded++;
            }
        }

        return animations;
    }

    std::shared_ptr<AnimationClip> ModelLoader::ProcessAnimation(const aiAnimation* aiAnim, std::shared_ptr<Skeleton> skeleton)
    {
        if (!aiAnim || !skeleton)
            return nullptr;

        std::string animName = aiAnim->mName.C_Str();
        if (animName.empty())
            animName = "Animation_" + std::to_string(m_LastStats.animationsLoaded);

        float duration = static_cast<float>(aiAnim->mDuration);
        float ticksPerSecond = static_cast<float>(aiAnim->mTicksPerSecond);

        //default to 25 fps if not specified
        if (ticksPerSecond <= 0.0f)
            ticksPerSecond = 25.0f;

        auto clip = std::make_shared<AnimationClip>(animName, duration);
        clip->SetTicksPerSecond(ticksPerSecond);

        //process each animation channel(bone animation Track)
        for (unsigned int i = 0; i < aiAnim->mNumChannels; i++)
        {
            const aiNodeAnim* channel = aiAnim->mChannels[i];
            std::string boneName = channel->mNodeName.C_Str();

            //check if this bone exists in our skeleton
            int boneIndex = skeleton->GetBoneIndex(boneName);
            if (boneIndex < 0)
            {
                //skip not in skeleton (might be helper nodes)
                continue;
            }

            //build Keyframes for this Bone
            std::vector<Keyframe> keyframes;

            // We need to handle the case where position, rotation, and scale
            // keys might not align. We'll create a unified keyframe list.
            std::set<float> allTimes;

            //collect all unique timestamps
            for (unsigned int p = 0; p < channel->mNumPositionKeys; p++)
                allTimes.insert(static_cast<float>(channel->mPositionKeys[p].mTime));
            for (unsigned int r = 0; r < channel->mNumRotationKeys; r++)
                allTimes.insert(static_cast<float>(channel->mRotationKeys[r].mTime));
            for (unsigned int s = 0; s < channel->mNumScalingKeys; s++)
                allTimes.insert(static_cast<float>(channel->mScalingKeys[s].mTime));

            //create keyframes for each unique time
            for (float time : allTimes)
            {
                Keyframe keyframe;
                keyframe.TimeStamp = time;

                // Find or interpolate position
                keyframe.Translation = FindOrInterpolatePosition(channel, time);

                // Find or interpolate rotation
                keyframe.Rotation = FindOrInterpolateRotation(channel, time);

                // Find or interpolate scale
                keyframe.Scale = FindOrInterpolateScale(channel, time);

                keyframes.push_back(keyframe);
            }
            //add this bone's animation to the clip
            if (!keyframes.empty())
            {
                clip->AddBoneAnimation(boneName, keyframes);
            }
        }

        return clip;
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

    DirectX::XMFLOAT4 ModelLoader::ConvertQuaternion(const aiQuaternion& quat)
    {
        return DirectX::XMFLOAT4(quat.x, quat.y, quat.z, quat.w);
    }

    DirectX::XMFLOAT3 ModelLoader::FindOrInterpolatePosition(const aiNodeAnim* channel, float time)
    {
        //if only one  key, return it 
       if (channel->mNumPositionKeys == 1)
        return ConvertVector3(channel->mPositionKeys[0].mValue);

    for (unsigned int i = 0; i < channel->mNumPositionKeys - 1; i++)
    {
        float t0 = static_cast<float>(channel->mPositionKeys[i].mTime);
        float t1 = static_cast<float>(channel->mPositionKeys[i + 1].mTime);

        if (time >= t0 && time < t1)
        {
            float factor = (time - t0) / (t1 - t0);
            const aiVector3D& v0 = channel->mPositionKeys[i].mValue;
            const aiVector3D& v1 = channel->mPositionKeys[i + 1].mValue;

            aiVector3D result = v0 + (v1 - v0) * factor;
            return ConvertVector3(result);
        }
    }

    return ConvertVector3(channel->mPositionKeys[channel->mNumPositionKeys - 1].mValue);
    }

    DirectX::XMFLOAT4 ModelLoader::FindOrInterpolateRotation(const aiNodeAnim* channel, float time)
    {
        if (channel->mNumRotationKeys == 1)
            return ConvertQuaternion(channel->mRotationKeys[0].mValue);

        for (unsigned int i = 0; i < channel->mNumRotationKeys - 1; i++)
        {
            float t0 = static_cast<float>(channel->mRotationKeys[i].mTime);
            float t1 = static_cast<float>(channel->mRotationKeys[i + 1].mTime);

            if (time >= t0 && time < t1)
            {
                float factor = (time - t0) / (t1 - t0);
                aiQuaternion q0 = channel->mRotationKeys[i].mValue;
                aiQuaternion q1 = channel->mRotationKeys[i + 1].mValue;

                aiQuaternion result;
                aiQuaternion::Interpolate(result, q0, q1, factor);
                result.Normalize();

                return ConvertQuaternion(result);
            }
        }

        return ConvertQuaternion(channel->mRotationKeys[channel->mNumRotationKeys - 1].mValue);
    }

    DirectX::XMFLOAT3 ModelLoader::FindOrInterpolateScale(const aiNodeAnim* channel, float time)
    {
        if (channel->mNumScalingKeys == 1)
            return ConvertVector3(channel->mScalingKeys[0].mValue);

        for (unsigned int i = 0; i < channel->mNumScalingKeys - 1; i++)
        {
            float t0 = static_cast<float>(channel->mScalingKeys[i].mTime);
            float t1 = static_cast<float>(channel->mScalingKeys[i + 1].mTime);

            if (time >= t0 && time < t1)
            {
                float factor = (time - t0) / (t1 - t0);
                const aiVector3D& v0 = channel->mScalingKeys[i].mValue;
                const aiVector3D& v1 = channel->mScalingKeys[i + 1].mValue;

                aiVector3D result = v0 + (v1 - v0) * factor;
                return ConvertVector3(result);
            }
        }

        return ConvertVector3(channel->mScalingKeys[channel->mNumScalingKeys - 1].mValue);
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
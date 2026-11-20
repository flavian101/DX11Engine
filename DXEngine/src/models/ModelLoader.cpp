#include "dxpch.h"
#include "ModelLoader.h"
#include "utils/Mesh/Utils/VertexAttribute.h"
#include "models/ModelAsset.h"
#include "utils/Mesh/Mesh.h"
#include "utils/Mesh/Resource/MeshResource.h"
#include "utils/material/Material.h"
#include "renderer/Renderer.h"
#include <chrono>
#include <algorithm>
#include "Animation/AnimationClip.h"
#include <set>
#include <assimp/mesh.h>
#include "processors/AnimationProcessor.h"
#include "processors/MaterialProcessor.h"
#include "processors/MeshProcessor.h"
#include "processors/ModelPostProcessor.h"
#include "processors/SkeletonProcessor.h"
#include "processors/TextureLoader.h"

namespace DXEngine
{

    class AssimpImporter
    {
    public:
        Assimp::Importer importer;
    };

    ModelLoader::ModelLoader()
        : ModelLoader(Renderer::GetDevice())
    {
    }

    ModelLoader::ModelLoader(std::shared_ptr<RHI::IGraphicsDevice> device)
        : m_Device(std::move(device))   // declaration order in ModelLoader.h
        , m_Importer(new AssimpImporter())
    {
        if (!m_Device)
            OutputDebugStringA("WARNING: ModelLoader created without a GPU device (call Renderer::Init first)\n");

        // Initialize processing components
        m_TextureLoader = std::make_shared<TextureLoader>(m_Device);
        m_MaterialProcessor = std::make_shared<MaterialProcessor>(m_TextureLoader);
        m_MeshProcessor = std::make_shared<MeshProcessor>();
        m_SkeletonProcessor = std::make_shared<SkeletonProcessor>();
        m_AnimationProcessor = std::make_shared<AnimationProcessor>();
        m_PostProcessor = std::make_shared<ModelPostProcessor>();

        m_Importer->importer.SetPropertyInteger(AI_CONFIG_PP_LBW_MAX_WEIGHTS, 4);
        // The engine's unit is the METRE: with aiProcess_GlobalScale (see BuildProcessingFlags) assimp rescales a file by
        // its own unit (an FBX in centimetres by 0.01; glTF is already in metres), to this many units per metre.
        m_Importer->importer.SetPropertyFloat(AI_CONFIG_GLOBAL_SCALE_FACTOR_KEY, 1.0f);
    }

    ModelLoader::~ModelLoader()
    {
        delete m_Importer;
    }

    std::shared_ptr<ModelAsset> ModelLoader::LoadModel(const std::string& filepath, const ModelLoadOptions& options)
    {
        auto start = std::chrono::high_resolution_clock::now();

        ClearError();
        m_Stats.Reset();

        //check cache first
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
                    m_ModelCache.erase(it);
                }
            }
        }
        // Validate file
        if (!ModelLoaderUtils::FileExists(filepath)) {
            SetError("File does not exist: " + filepath);
            return nullptr;
        }

        // Set up Assimp post-processing flags
        unsigned int flags = BuildProcessingFlags(options);

        // Load scene
        const aiScene* scene = m_Importer->importer.ReadFile(filepath, flags);

        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
            SetError("Assimp error: " + std::string(m_Importer->importer.GetErrorString()));
            return nullptr;
        }

        //store context 
        m_CurrentScene = scene;
        m_CurrentDirectory = ModelLoaderUtils::GetDirectory(filepath);


        //process The scene
        // let the texture loader resolve embedded textures ("*0") of THIS scene, only while processing it
        m_TextureLoader->SetCurrentScene(scene);
        auto model = ProcessScene(scene, m_CurrentDirectory, options);
        m_TextureLoader->SetCurrentScene(nullptr);

        if (model)
        {
            model->sourcePath = filepath;
            model->name = std::filesystem::path(filepath).filename().string();

            // Post-process the model (its problems are listed in model->warnings)
            m_PostProcessor->PostProcess(model, options);

            // Update statistics
            m_Stats.meshesLoaded = static_cast<uint32_t>(m_MeshProcessor->GetMeshesProcessed());
            m_Stats.materialsLoaded = static_cast<uint32_t>(m_MaterialProcessor->GetMaterialsProcessed());
            m_Stats.texturesLoaded = static_cast<uint32_t>(m_TextureLoader->GetTexturesLoaded());
            m_Stats.animationsLoaded = static_cast<uint32_t>(m_AnimationProcessor->GetAnimationsProcessed());
            m_Stats.bonesLoaded = static_cast<uint32_t>(m_SkeletonProcessor->GetBonesProcessed());
            m_Stats.memoryUsed = model->GetMemoryUsage();

            // Cache result
            if (m_CachingEnabled) {
                std::string cacheKey = GenerateCacheKey(filepath, options);
                std::lock_guard<std::mutex> lock(m_CacheMutex);
                m_ModelCache[cacheKey] = model;
            }
        }

        // Calculate load time
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        m_Stats.loadTimeSeconds = duration.count() / 1000.0f;

        return model;
    }

    std::vector<std::shared_ptr<AnimationClip>> ModelLoader::LoadAnimations(const std::string& filepath)
    {
        std::vector<std::shared_ptr<AnimationClip>> animations;

        ClearError();

        if (!ModelLoaderUtils::FileExists(filepath)) {
            SetError("File does not exist: " + filepath);
            return animations;
        }

        // Load the scene
        const aiScene* scene = m_Importer->importer.ReadFile(filepath,
            aiProcess_ValidateDataStructure | aiProcess_PopulateArmatureData);

        if (!scene || !scene->HasAnimations()) {
            if (!scene) {
                SetError("Assimp error: " + std::string(m_Importer->importer.GetErrorString()));
            }
            else {
                SetError("No animations found in file");
            }
            return animations;
        }

        // Find skeleton from first skinned mesh
        std::shared_ptr<Skeleton> skeleton;
        for (unsigned int i = 0; i < scene->mNumMeshes && !skeleton; i++) {
            const aiMesh* mesh = scene->mMeshes[i];
            if (mesh->HasBones()) {
                skeleton = m_SkeletonProcessor->ProcessSkeleton(scene, mesh);
                break;
            }
        }

        if (!skeleton) {
            SetError("Cannot load animations without a skeleton");
            return animations;
        }

        // Process animations using AnimationProcessor
        animations = m_AnimationProcessor->ProcessAnimations(scene, skeleton);

        return animations;
    }

    std::shared_ptr<ModelAsset> ModelLoader::ProcessScene(
        const aiScene* scene,
        const std::string& directory,
        const ModelLoadOptions& options)
    {
        // Reset per-model state
        m_CurrentSkeleton.reset();
        m_MaterialCache.clear();

        auto asset = std::make_shared<ModelAsset>();

        // ---- skeleton: the mesh with the most bones has the most complete one
        const bool hasAnimations = scene->HasAnimations() && options.loadAnimations;
        const aiMesh* bestSkeletonMesh = nullptr;
        unsigned int maxBones = 0;
        for (unsigned int i = 0; i < scene->mNumMeshes; i++)
        {
            const aiMesh* mesh = scene->mMeshes[i];
            if (mesh->HasBones() && mesh->mNumBones > maxBones)
            {
                maxBones = mesh->mNumBones;
                bestSkeletonMesh = mesh;
            }
        }
        if (bestSkeletonMesh)
        {
            m_CurrentSkeleton = m_SkeletonProcessor->ProcessSkeleton(scene, bestSkeletonMesh);
            if (!m_CurrentSkeleton)
                asset->warnings.push_back("The model has skinning data but its skeleton could not be built: it is drawn without bones");
        }
        asset->skeleton = m_CurrentSkeleton;

        // ---- node tree and meshes (a mesh used by several nodes is built once)
        std::unordered_map<unsigned int, uint32_t> meshOfAssimpMesh;
        BuildNodes(*asset, scene->mRootNode, -1, scene, directory, options, meshOfAssimpMesh);

        // ---- animations: only with a skeleton to drive
        if (hasAnimations && m_CurrentSkeleton)
        {
            asset->clips = m_AnimationProcessor->ProcessAnimations(scene, m_CurrentSkeleton);
        }
        else if (hasAnimations && !m_CurrentSkeleton)
        {
            asset->warnings.push_back("The file has animation but no skeleton: the animation was ignored");
        }

        return asset;
    }

    uint32_t ModelLoader::BuildNodes(ModelAsset& asset, const aiNode* node, int parent, const aiScene* scene,
        const std::string& directory, const ModelLoadOptions& options,
        std::unordered_map<unsigned int, uint32_t>& meshOfAssimpMesh)
    {
        std::string nodeName = node->mName.C_Str();
        if (nodeName.empty())
            nodeName = parent < 0 ? "Root" : "Node_" + std::to_string(asset.nodes.size());

        const uint32_t index = asset.AddNode(nodeName, parent, ConvertMatrix(node->mTransformation));

        for (unsigned int i = 0; i < node->mNumMeshes; i++)
        {
            const unsigned int assimpIndex = node->mMeshes[i];
            auto known = meshOfAssimpMesh.find(assimpIndex);
            if (known != meshOfAssimpMesh.end())
            {
                asset.AttachMesh(index, known->second);
                continue;
            }

            const aiMesh* aiMesh = scene->mMeshes[assimpIndex];
            std::shared_ptr<MeshResource> meshResource =
                m_MeshProcessor->ProcessMesh(aiMesh, scene, m_CurrentSkeleton, options);
            if (!meshResource)
            {
                asset.warnings.push_back("Mesh '" + std::string(aiMesh->mName.C_Str()) + "' could not be imported and is missing");
                continue;
            }

            auto meshObject = std::make_shared<Mesh>(meshResource, m_Device);

            // one Material per assimp material: meshes that use the same one share it
            if (options.loadMaterials)
            {
                auto material = GetMaterial(scene, aiMesh->mMaterialIndex, directory, options, asset);
                if (material)
                    meshObject->SetMaterial(material);
            }

            std::string meshName = aiMesh->mName.C_Str();
            if (meshName.empty())
                meshName = "Mesh_" + std::to_string(asset.meshes.size());

            const uint32_t meshIndex = asset.AddMesh(meshName, std::move(meshObject));
            meshOfAssimpMesh[assimpIndex] = meshIndex;
            asset.AttachMesh(index, meshIndex);
        }

        for (unsigned int i = 0; i < node->mNumChildren; i++)
            BuildNodes(asset, node->mChildren[i], static_cast<int>(index), scene, directory, options, meshOfAssimpMesh);
        return index;
    }

    std::shared_ptr<Material> ModelLoader::GetMaterial(const aiScene* scene, unsigned int materialIndex,
        const std::string& directory, const ModelLoadOptions& options, ModelAsset& asset)
    {
        if (materialIndex >= scene->mNumMaterials)
            return nullptr;

        auto cached = m_MaterialCache.find(materialIndex);
        if (cached != m_MaterialCache.end())
            return cached->second;

        auto material = m_MaterialProcessor->ProcessMaterial(scene->mMaterials[materialIndex], directory, options, &asset.warnings);
        if (!material)
            asset.warnings.push_back("Material " + std::to_string(materialIndex) + " could not be imported: its meshes use a default material");
        else
            asset.materials.push_back(material);
        m_MaterialCache[materialIndex] = material;
        return material;
    }

    bool ModelLoader::ValidateFile(const std::string& filePath)
    {
        if (!ModelLoaderUtils::FileExists(filePath)) {
            SetError("File does not exist: " + filePath);
            return false;
        }

        std::string extension = ModelLoaderUtils::GetFileExtension(filePath);
        if (!ModelLoaderUtils::IsSupportedFormat(extension)) {
            SetError("Unsupported file format: " + extension);
            return false;
        }

        return true;
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
        if (!ValidateFile(filePath)) {
            return "Invalid file: " + GetLastError();
        }

        const aiScene* scene = m_Importer->importer.ReadFile(filePath,
            aiProcess_ValidateDataStructure | aiProcess_PopulateArmatureData);

        if (!scene) {
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

    void ModelLoader::EnableCaching(bool enable)
    {
        m_CachingEnabled = enable;
        m_TextureLoader->EnableCaching(enable);
    }

    void ModelLoader::ClearCache()
    {
        std::lock_guard<std::mutex> lock(m_CacheMutex);
        m_ModelCache.clear();
        m_TextureLoader->ClearCache();
    }

    // ============================================================================
    // PRIVATE HELPER METHODS
    // ============================================================================

    unsigned int ModelLoader::BuildProcessingFlags(const ModelLoadOptions& options) const
    {
        unsigned int flags = aiProcess_ValidateDataStructure | aiProcess_GlobalScale;   // GlobalScale: the file's unit -> metres

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

        return flags;
    }

    std::string ModelLoader::GenerateCacheKey(
        const std::string& filePath,
        const ModelLoadOptions& options)
    {
        // EVERY option that changes the result is part of the key (it used to be three of them: a model loaded with
        // different options came back from the cache as the first one)
        std::string key = std::filesystem::absolute(filePath).lexically_normal().string() + "|";
        for (bool flag : { options.makeLeftHanded, options.generateNormals, options.generateTangents, options.flipUVs,
                           options.optimizeMeshes, options.validateDataStructure, options.triangulate, options.loadAnimations,
                           options.loadMaterials, options.loadTextures, options.joinIdenticalVertices,
                           options.removeRedundantMaterials, options.fixInfacingNormals, options.limitBoneWeights })
            key += flag ? '1' : '0';
        key += "|" + std::to_string(options.globalScale) + "|" + std::to_string(options.maxBoneWeights);
        return key;
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

    // ============================================================================
    // CONVERSION UTILITIES
    // ============================================================================

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

    // ============================================================================
    // STATISTICS
    // ============================================================================

    std::string ModelLoader::LoadStatistics::ToString() const
    {
        std::ostringstream oss;
        oss << "=== Model Load Statistics ===\n";
        oss << "Load Time: " << loadTimeSeconds << " seconds\n";
        oss << "Meshes: " << meshesLoaded << "\n";
        oss << "Materials: " << materialsLoaded << "\n";
        oss << "Textures: " << texturesLoaded << "\n";
        oss << "Animations: " << animationsLoaded << "\n";
        oss << "Bones: " << bonesLoaded << "\n";
        oss << "Memory Used: " << memoryUsed << " bytes\n";
        return oss.str();
    }

}
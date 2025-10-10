#include "dxpch.h"
#include "ModelLoader.h"
#include "utils/Mesh/Utils/VertexAttribute.h"
#include "models/Model.h"
#include "utils/Mesh/Mesh.h"
#include "utils/Mesh/Resource/MeshResource.h"
#include "utils/material/Material.h"
#include "utils/Texture.h"
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
        :m_Importer(new AssimpImporter())
    {
        // Initialize processing components
        m_TextureLoader = std::make_shared<TextureLoader>();
        m_MaterialProcessor = std::make_shared<MaterialProcessor>(m_TextureLoader);
        m_MeshProcessor = std::make_shared<MeshProcessor>();
        m_SkeletonProcessor = std::make_shared<SkeletonProcessor>();
        m_AnimationProcessor = std::make_shared<AnimationProcessor>();
        m_PostProcessor = std::make_shared<ModelPostProcessor>();

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
        auto model = ProcessScene(scene, m_CurrentDirectory, options);

        if (model)
        {
            // Post-process the model
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

    std::shared_ptr<Model> ModelLoader::ProcessScene(
        const aiScene* scene,
        const std::string& directory,
        const ModelLoadOptions& options)
    {
        // Reset current skeleton for new model
        m_CurrentSkeleton.reset();

        bool hasAnimations = scene->HasAnimations() && options.loadAnimations;
        bool hasSkinning = false;

        // FIX: First pass - detect if ANY mesh has bones
        for (unsigned int i = 0; i < scene->mNumMeshes; i++)
        {
            if (scene->mMeshes[i]->HasBones())
            {
                hasSkinning = true;
                break;
            }
        }

        auto model = std::make_shared<Model>();

        // FIX: Process skeleton ONLY if skinning detected
        if (hasSkinning)
        {
            // Find the mesh with the most bones (most complete skeleton)
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

                if (!m_CurrentSkeleton) {
                    OutputDebugStringA("Warning: Model has skinning data but skeleton creation failed\n");
                    hasSkinning = false; // Disable skinning if skeleton failed
                }
                else {
#ifdef DX_DEBUG
                    OutputDebugStringA(("Skeleton created with " +
                        std::to_string(m_CurrentSkeleton->GetBoneCount()) +
                        " bones from mesh with " + std::to_string(maxBones) +
                        " bone references\n").c_str());
#endif
                }
            }
        }

        // Enable skinning on model if we have a valid skeleton
        if (m_CurrentSkeleton)
        {
            model->EnableSkinning(m_CurrentSkeleton);
        }

        // Process meshes (will now correctly handle skinned vs non-skinned)
        ProcessNode(model, scene->mRootNode, scene, directory, options);

        // Load Animations (only if we have both animations AND a skeleton)
        if (hasAnimations && m_CurrentSkeleton)
        {
            auto animations = m_AnimationProcessor->ProcessAnimations(scene, m_CurrentSkeleton);

            for (const auto& clip : animations)
            {
                model->AddAnimationClip(clip);
            }

#ifdef DX_DEBUG
            OutputDebugStringA(("Loaded " + std::to_string(animations.size()) +
                " animation(s) for model\n").c_str());
#endif

            if (!animations.empty())
            {
                auto controller = std::make_shared<AnimationController>(m_CurrentSkeleton);
                controller->SetClip(animations[0]);
                controller->SetPlaybackMode(PlaybackMode::Loop);
                controller->Play();
                model->SetAnimationController(controller);
            }
        }
        else if (hasSkinning && !m_CurrentSkeleton)
        {
            OutputDebugStringA("Warning: Model has skinning data but no skeleton was created\n");
        }
        else if (hasAnimations && !m_CurrentSkeleton)
        {
            OutputDebugStringA("Warning: Model has animations but no skeleton - animations will be ignored\n");
        }

        // Apply global scale
        if (options.globalScale != 1.0f) {
            DirectX::XMFLOAT3 scale(options.globalScale, options.globalScale, options.globalScale);
            model->SetScale(scale);
        }

        return model;
    }

    void ModelLoader::ProcessNode(std::shared_ptr<Model> model,const aiNode* node,const aiScene* scene,const std::string& directory,const ModelLoadOptions& options)
    {
        // Process all meshes in this node
        for (unsigned int i = 0; i < node->mNumMeshes; i++) {
            const aiMesh* aiMesh = scene->mMeshes[node->mMeshes[i]];

            // Process mesh geometry using MeshProcessor
            std::shared_ptr<MeshResource> meshResource =
                m_MeshProcessor->ProcessMesh(aiMesh, scene, m_CurrentSkeleton, options);

            if (meshResource) {
                auto meshObject = std::make_shared<Mesh>(meshResource);

                // Process material using MaterialProcessor
                if (options.loadMaterials && aiMesh->mMaterialIndex >= 0) {
                    auto material = m_MaterialProcessor->ProcessMaterial(
                        scene->mMaterials[aiMesh->mMaterialIndex],
                        directory,
                        options);

                    if (material) {
                        meshObject->SetMaterial(material);
                    }
                }

                // Add mesh to model
                std::string meshName = aiMesh->mName.C_Str();
                if (meshName.empty())
                    meshName = "Mesh_" + std::to_string(model->GetMeshCount());

                model->AddMesh(meshObject, meshName);
            }
        }

        // Process child nodes recursively
        for (unsigned int i = 0; i < node->mNumChildren; i++) {
            ProcessNode(model, node->mChildren[i], scene, directory, options);
        }
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

        return flags;
    }

    std::string ModelLoader::GenerateCacheKey(
        const std::string& filePath,
        const ModelLoadOptions& options)
    {
        return filePath + "_" + std::to_string(
            options.generateNormals + options.generateTangents * 2 + options.flipUVs * 4);
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
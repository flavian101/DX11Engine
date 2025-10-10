#pragma once
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <DirectXMath.h>
#include <mutex>
#include <functional> 
#include "processors/ModelLoaderUtils.h"

#include <assimp/material.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/matrix4x4.h>
#include <assimp/vector3.h>
#include <assimp/color4.h>



namespace DXEngine {

    // Forward declarations
    class Model;
    class Mesh;
    class Material;
    class MeshResource;
    class Texture;
    class Skeleton;
    class AnimationClip;
    struct Keyframe;
    enum class MaterialType;
    class TextureLoader;
    class MaterialProcessor;
    class MeshProcessor;
    class SkeletonProcessor;
    class AnimationProcessor;
    class ModelPostProcessor;

    class ModelLoader
    {
    public:
        ModelLoader();
        ~ModelLoader();

        // Main loading methods
        std::shared_ptr<Model> LoadModel(const std::string& filePath,
            const ModelLoadOptions& options = ModelLoadOptions{});

        std::vector<std::shared_ptr<AnimationClip>> LoadAnimations(const std::string& filepath);

        // Validation and information
        bool ValidateFile(const std::string& filePath);
        std::vector<std::string> GetSupportedFormats() const;
        std::string GetFileInfo(const std::string& filePath);

        // Cache management
        void EnableCaching(bool enable);
        void ClearCache();
        size_t GetCacheSize() const { return m_ModelCache.size(); }

        // Error handling
        const std::string& GetLastError() const { return m_LastError; }
        bool HasError() const { return !m_LastError.empty(); }

        // Statistics
        struct LoadStatistics
        {
            uint32_t meshesLoaded = 0;
            uint32_t materialsLoaded = 0;
            uint32_t texturesLoaded = 0;
            uint32_t animationsLoaded = 0;
            uint32_t bonesLoaded = 0;
            float loadTimeSeconds = 0.0f;
            size_t memoryUsed = 0;

            void Reset()
            {
                meshesLoaded = materialsLoaded = texturesLoaded = 0;
                animationsLoaded = bonesLoaded = 0;
                loadTimeSeconds = 0.0f;
                memoryUsed = 0;
            }

            std::string ToString() const;
        };

        const LoadStatistics& GetLastLoadStats() const { return m_Stats; }
    private:
        // Core processing methods
        std::shared_ptr<Model> ProcessScene(const aiScene* scene, const std::string& directory,
            const ModelLoadOptions& options);

        void ProcessNode(std::shared_ptr<Model> model, const aiNode* node, const aiScene* scene,
            const std::string& directory, const ModelLoadOptions& options);
 
        // Utility methods
        std::string GetTextureFilename(const aiMaterial* material, aiTextureType type,
            const std::string& directory);

        DirectX::XMFLOAT4X4 ConvertMatrix(const aiMatrix4x4& matrix);
        DirectX::XMFLOAT3 ConvertVector3(const aiVector3D& vector);
        DirectX::XMFLOAT4 ConvertColor(const aiColor4D& color);
        DirectX::XMFLOAT4 ConvertQuaternion(const aiQuaternion& quat);

        // Error handling
        void SetError(const std::string& error);
        void ClearError();

        unsigned int BuildProcessingFlags(const ModelLoadOptions& options) const;

        // Caching
        std::string GenerateCacheKey(const std::string& filePath, const ModelLoadOptions& options);

    private:
        // Processing components
        std::shared_ptr<TextureLoader> m_TextureLoader;
        std::shared_ptr<MaterialProcessor> m_MaterialProcessor;
        std::shared_ptr<MeshProcessor> m_MeshProcessor;
        std::shared_ptr<SkeletonProcessor> m_SkeletonProcessor;
        std::shared_ptr<AnimationProcessor> m_AnimationProcessor;
        std::shared_ptr<ModelPostProcessor> m_PostProcessor;

        // Assimp
        class AssimpImporter* m_Importer;

        // Current loading context
        const aiScene* m_CurrentScene = nullptr;
        std::string m_CurrentDirectory;
        std::shared_ptr<Skeleton> m_CurrentSkeleton;

        // Cache
        std::unordered_map<std::string, std::weak_ptr<Model>> m_ModelCache;
        bool m_CachingEnabled = true;
        mutable std::mutex m_CacheMutex;

        // State
        std::string m_LastError;
        LoadStatistics m_Stats;
    };

    // Utility functions
    
}
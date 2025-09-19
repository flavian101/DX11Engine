#pragma once
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <DirectXMath.h>
#include <mutex>

#include <assimp/scene.h>
#include <assimp/mesh.h>
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
    class SkinnedMeshResource;
    class Texture;

    struct LoadedAnimation
    {
        std::string name;
        float duration;
        float ticksPerSecond;
        std::vector<DirectX::XMFLOAT4X4> boneTransforms;
    };

    struct LoadedBone
    {
        std::string name;
        DirectX::XMFLOAT4X4 offsetMatrix;
        int32_t parentIndex = -1;
    };

    struct ModelLoadOptions
    {
        bool makeLeftHanded = true;
        bool generateNormals = false;
        bool generateTangents = true;
        bool flipUVs = false;
        bool optimizeMeshes = true;
        bool validateDataStructure = true;
        bool triangulate = true;
        bool loadAnimations = false;
        bool loadMaterials = true;
        bool loadTextures = true;
        float globalScale = 1.0f;

        // Optimization settings
        bool joinIdenticalVertices = true;
        bool removeRedundantMaterials = true;
        bool fixInfacingNormals = true;
        bool limitBoneWeights = true;
        uint32_t maxBoneWeights = 4;
    };

    class ModelLoader
    {
    public:
        ModelLoader();
        ~ModelLoader();

        // Main loading methods
        std::shared_ptr<Model> LoadModel(const std::string& filePath,
            const ModelLoadOptions& options = ModelLoadOptions{});

        std::shared_ptr<Model> LoadModelAsync(const std::string& filePath,
            const ModelLoadOptions& options = ModelLoadOptions{});

        // Specialized loading methods
        std::shared_ptr<MeshResource> LoadMeshResource(const std::string& filePath,
            const ModelLoadOptions& options = ModelLoadOptions{});

        std::vector<std::shared_ptr<Texture>> LoadTextures(const std::string& filePath);
        std::vector<LoadedAnimation> LoadAnimations(const std::string& filePath);

        // Batch loading
        std::vector<std::shared_ptr<Model>> LoadModels(const std::vector<std::string>& filePaths,
            const ModelLoadOptions& options = ModelLoadOptions{});

        // Validation and information
        bool ValidateFile(const std::string& filePath);
        std::vector<std::string> GetSupportedFormats() const;
        std::string GetFileInfo(const std::string& filePath);

        // Cache management
        void EnableCaching(bool enable) { m_CachingEnabled = enable; }
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
        };

        const LoadStatistics& GetLastLoadStats() const { return m_LastStats; }

    private:
        // Core processing methods
        std::shared_ptr<Model> ProcessScene(const aiScene* scene, const std::string& directory,
            const ModelLoadOptions& options);

        void ProcessNode(std::shared_ptr<Model> model, const aiNode* node, const aiScene* scene,
            const std::string& directory, const ModelLoadOptions& options);

        std::shared_ptr<MeshResource> ProcessMesh(const aiMesh* mesh, const aiScene* scene,
            const ModelLoadOptions& options);

        std::shared_ptr<Material> ProcessMaterial(const aiMaterial* material,
            const std::string& directory, const ModelLoadOptions& options);

        // Specialized processing
        std::shared_ptr<SkinnedMeshResource> ProcessSkinnedMesh(const aiMesh* mesh,
            const aiScene* scene, const ModelLoadOptions& options);

        std::vector<LoadedAnimation> ProcessAnimations(const aiScene* scene);
        std::vector<LoadedBone> ProcessBones(const aiMesh* mesh);

        // Texture loading
        std::shared_ptr<Texture> LoadTexture(const std::string& path, aiTextureType type);
        std::shared_ptr<Texture> LoadEmbeddedTexture(const aiScene* scene, const std::string& path);

        // Utility methods
        std::string GetTextureFilename(const aiMaterial* material, aiTextureType type,
            const std::string& directory);

        DirectX::XMFLOAT4X4 ConvertMatrix(const aiMatrix4x4& matrix);
        DirectX::XMFLOAT3 ConvertVector3(const aiVector3D& vector);
        DirectX::XMFLOAT4 ConvertColor(const aiColor4D& color);

        // Post-processing
        void PostProcessModel(std::shared_ptr<Model> model, const ModelLoadOptions& options);
        void OptimizeModel(std::shared_ptr<Model> model);
        void ValidateModel(std::shared_ptr<Model> model);

        // Error handling
        void SetError(const std::string& error);
        void ClearError();

        // Caching
        std::string GenerateCacheKey(const std::string& filePath, const ModelLoadOptions& options);

    private:
        // Cache for loaded models
        std::unordered_map<std::string, std::weak_ptr<Model>> m_ModelCache;
        std::unordered_map<std::string, std::weak_ptr<Texture>> m_TextureCache;
        std::unordered_map<std::string, std::weak_ptr<Material>> m_MaterialCache;

        bool m_CachingEnabled = true;
        std::string m_LastError;
        LoadStatistics m_LastStats;

        // Assimp importer instance
        class AssimpImporter* m_Importer;

        // Current loading context
        std::string m_CurrentDirectory;
        const aiScene* m_CurrentScene = nullptr;

        // Threading support
        mutable std::mutex m_CacheMutex;
    };

    // Utility functions
    namespace ModelLoaderUtils
    {
        // Format detection
        bool IsSupportedFormat(const std::string& extension);
        std::string GetFormatDescription(const std::string& extension);

        // Path utilities
        std::string GetFileExtension(const std::string& filePath);
        std::string GetFileName(const std::string& filePath);
        std::string GetDirectory(const std::string& filePath);
        std::string NormalizePath(const std::string& path);

        // Validation
        bool FileExists(const std::string& filePath);
        bool IsValidModelFile(const std::string& filePath);

        // Options presets
        ModelLoadOptions GetGameOptimizedOptions();
        ModelLoadOptions GetHighQualityOptions();
        ModelLoadOptions GetFastLoadOptions();
    }
}
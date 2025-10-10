#pragma once
#include "dxpch.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <filesystem>   


namespace DXEngine
{
    struct ModelLoadOptions
    {
        bool makeLeftHanded = true;
        bool generateNormals = true;
        bool generateTangents = true;
        bool flipUVs = false;
        bool optimizeMeshes = true;
        bool validateDataStructure = true;
        bool triangulate = true;
        bool loadAnimations = true;
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


    namespace ModelLoaderUtils
    {
        std::string GetFileExtension(const std::string& filePath);
        bool IsSupportedFormat(const std::string& extension);

        inline bool FileExists(const std::string& filePath)
        {
            return std::filesystem::exists(filePath);
        }

        inline std::string GetDirectory(const std::string& filePath)
        {
            return std::filesystem::path(filePath).parent_path().string();
        }
        inline std::string GetFormatDescription(const std::string& extension)
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

        inline std::string GetFileName(const std::string& filePath)
        {
            return std::filesystem::path(filePath).filename().string();
        }

        inline bool IsValidModelFile(const std::string& filePath)
        {
            if (!FileExists(filePath))
                return false;

            std::string ext = GetFileExtension(filePath);
            return IsSupportedFormat(ext);
        }

        inline std::string GetFileExtension(const std::string& filePath)
        {
            return std::filesystem::path(filePath).extension().string();
        }

        inline std::string NormalizePath(const std::string& path)
        {
            return std::filesystem::path(path).lexically_normal().string();
        }

        inline ModelLoadOptions GetGameOptimizedOptions()
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

        inline ModelLoadOptions GetHighQualityOptions()
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

        inline ModelLoadOptions GetFastLoadOptions()
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

        inline bool IsSupportedFormat(const std::string& extension)
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
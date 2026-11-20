#pragma once
// ============================================================================
// AssetCatalog: which model files lie in the assets folder. Pure logic over std::filesystem (no engine types, no GPU, no
// ImGui), so it is tested natively (tests/asset_catalog_tests.cpp).
// ============================================================================
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace DXEngine::Assets
{
    // The model formats the importer reads (one list: ModelLoaderUtils::IsSupportedFormat asks this function).
    // 'extension' with or without the dot, any case.
    bool IsModelExtension(const std::string& extension);

    // The same list as a Win32 file dialog filter and as text, for menus and messages: ".fbx .obj .gltf ..."
    std::vector<std::string> ModelExtensions();            // ".fbx", ".obj", ... (lower case, with the dot)

    struct ModelFile
    {
        std::string path;        // relative to the scanned folder, '/' separators ("ship/dutch_ship.fbx")
        std::string name;        // the file name ("dutch_ship.fbx")
        std::string folder;      // the folders above it ("ship"), empty for a file directly in the root
        uint64_t    sizeBytes = 0;
    };

    // Every model file under 'rootDirectory', recursively, sorted by folder then name (case-insensitive). A folder that does
    // not exist, or cannot be read, gives an empty list. At most 'maxFiles' entries (a runaway folder must not stall the UI).
    std::vector<ModelFile> ScanModelFiles(const std::string& rootDirectory, size_t maxFiles = 10000);

    // The indices of the files whose path contains 'query' (case-insensitive; empty = all), in the list's order.
    std::vector<size_t> FilterModelFiles(const std::vector<ModelFile>& files, const std::string& query);

    // ---- paths
    std::string FileStem(const std::string& path);         // "a/b/ship.fbx" -> "ship"
    std::string FormatBytes(uint64_t bytes);               // 1536 -> "1.5 KB"

    // One name for one file: absolute, lexically normalised, '/' separators, and lower-cased on a case-insensitive
    // file system (Windows). Two spellings of the same path give the same key.
    std::string NormalizePath(const std::string& path);
}

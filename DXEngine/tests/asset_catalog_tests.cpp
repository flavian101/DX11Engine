// AssetCatalog: which model files lie in the assets folder. Pure logic over std::filesystem.
#include "assets/AssetCatalog.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

using namespace DXEngine::Assets;
namespace fs = std::filesystem;

static int g_failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); ++g_failures; } } while (0)

static void Touch(const fs::path& file, size_t bytes = 10)
{
    fs::create_directories(file.parent_path());
    std::ofstream(file, std::ios::binary) << std::string(bytes, 'x');
}

static void Extensions()
{
    CHECK(IsModelExtension(".fbx") && IsModelExtension("fbx") && IsModelExtension(".FBX") && IsModelExtension(".GlTf"));
    CHECK(IsModelExtension(".glb") && IsModelExtension(".obj") && IsModelExtension(".dae"));
    CHECK(!IsModelExtension(".png") && !IsModelExtension("") && !IsModelExtension(".") && !IsModelExtension(".txt"));
    CHECK(!IsModelExtension(".fbxx") && !IsModelExtension("fbx.png"));
    CHECK(ModelExtensions().size() >= 5);
    for (const std::string& e : ModelExtensions()) CHECK(IsModelExtension(e) && e[0] == '.' && e == std::string(e.begin(), e.end()));
}

static void ScansRecursivelySortedAndRelative()
{
    const fs::path root = fs::temp_directory_path() / "dx_catalog_test";
    fs::remove_all(root);
    Touch(root / "zebra.fbx", 2048);
    Touch(root / "Apple.OBJ", 100);
    Touch(root / "ship" / "hull.glb", 1500000);
    Touch(root / "ship" / "textures" / "hull.png");            // not a model
    Touch(root / "ship" / "readme.txt");                       // not a model
    Touch(root / "ship" / "sails" / "main.gltf", 20);
    Touch(root / "Ship Parts" / "mast.dae");                   // a folder with a space

    const auto files = ScanModelFiles(root.string());
    std::vector<std::string> paths;
    for (const auto& f : files) paths.push_back(f.path);
    // tree order: the root's files, then each folder with its subfolders before the next folder ("ship/sails" belongs
    // with "ship", not after "Ship Parts"); names case-insensitive
    const std::vector<std::string> expected = { "Apple.OBJ", "zebra.fbx", "ship/hull.glb", "ship/sails/main.gltf", "Ship Parts/mast.dae" };
    CHECK(paths == expected);
    if (files.size() == 5)
    {
        CHECK(files[0].name == "Apple.OBJ" && files[0].folder.empty() && files[0].sizeBytes == 100);
        CHECK(files[1].sizeBytes == 2048);
        CHECK(files[2].folder == "ship" && files[2].name == "hull.glb" && files[2].sizeBytes == 1500000);
        CHECK(files[3].folder == "ship/sails");                // '/' separators, whatever the platform
        for (const auto& f : files) CHECK(f.path.find('\\') == std::string::npos);
    }

    // the filter
    CHECK(FilterModelFiles(files, "").size() == files.size());
    const auto ship = FilterModelFiles(files, "SHIP");
    CHECK(ship.size() == 3);                                   // "Ship Parts/mast.dae", "ship/hull.glb", "ship/sails/main.gltf"
    CHECK(FilterModelFiles(files, "main").size() == 1 && files[FilterModelFiles(files, "main")[0]].name == "main.gltf");
    CHECK(FilterModelFiles(files, "nothing like this").empty());
    CHECK(FilterModelFiles(files, "ship/").size() == 2);       // the folder is part of the path searched

    // a cap on the number of entries
    CHECK(ScanModelFiles(root.string(), 2).size() == 2);
    CHECK(ScanModelFiles(root.string(), 0).empty());
    fs::remove_all(root);
}

static void MissingOrEmptyFoldersAreEmptyLists()
{
    CHECK(ScanModelFiles("").empty());
    CHECK(ScanModelFiles((fs::temp_directory_path() / "dx_catalog_does_not_exist_123").string()).empty());
    const fs::path empty = fs::temp_directory_path() / "dx_catalog_empty";
    fs::create_directories(empty);
    CHECK(ScanModelFiles(empty.string()).empty());
    Touch(empty / "file.fbx");
    CHECK(ScanModelFiles((empty / "file.fbx").string()).empty());      // a file, not a folder
    fs::remove_all(empty);
}

static void Names()
{
    CHECK(FileStem("a/b/ship.fbx") == "ship" && FileStem("ship.tar.gz") == "ship.tar" && FileStem("ship") == "ship" && FileStem("").empty());

    CHECK(FormatBytes(0) == "0 B" && FormatBytes(1023) == "1023 B");
    CHECK(FormatBytes(1024) == "1.0 KB" && FormatBytes(1536) == "1.5 KB");
    CHECK(FormatBytes(5ull * 1024 * 1024) == "5.0 MB" && FormatBytes(3ull * 1024 * 1024 * 1024) == "3.0 GB");
    CHECK(FormatBytes(~0ull).find("TB") != std::string::npos);        // no unit past TB, no overflow
}

static void OneFileOneKey()
{
    CHECK(NormalizePath("a/b/../c/model.fbx") == NormalizePath("a/c/model.fbx"));
    CHECK(NormalizePath("a//c/./model.fbx") == NormalizePath("a/c/model.fbx"));
    CHECK(NormalizePath("a\\c\\model.fbx") == NormalizePath("a/c/model.fbx") || fs::path::preferred_separator == '/');   // (a backslash is a separator on Windows only)
    const std::string key = NormalizePath("relative/model.fbx");
    CHECK(fs::path(key).is_absolute());
    CHECK(key.find('\\') == std::string::npos);
    CHECK(NormalizePath("x/model.fbx") != NormalizePath("y/model.fbx"));
}

int main()
{
    Extensions();
    ScansRecursivelySortedAndRelative();
    MissingOrEmptyFoldersAreEmptyLists();
    Names();
    OneFileOneKey();
    if (g_failures) { std::printf("%d check(s) failed\n", g_failures); return EXIT_FAILURE; }
    std::printf("asset_catalog_tests: all passed\n");
    return EXIT_SUCCESS;
}

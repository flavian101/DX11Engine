// ProjectRoot: finding the folder that has assets/shaders, from the executable's folder or the working directory. Temp folders only.
#include "assets/ProjectRoot.h"
#include <cstdio>
#include <fstream>

using namespace DXEngine;
namespace fs = std::filesystem;

static int g_failed = 0;
#define CHECK(c) do { if (!(c)) { std::printf("  FAILED line %d: %s\n", __LINE__, #c); ++g_failed; } } while (0)

// <tmp>/dx_project_root_tests/
//   game/assets/shaders/        a root
//   game/build/bin/             no assets: its parents lead to the root
//   game/build/bin/Debug/       one level deeper
//   other/assets/shaders/       another root
//   empty/deep/er/              no root above it (inside the temp folder, which has none)
//   notadir/assets/shaders      a FILE called shaders: not a root
struct Tree
{
    Tree()
    {
        base = fs::temp_directory_path() / "dx_project_root_tests";
        fs::remove_all(base);
        fs::create_directories(base / "game" / "assets" / "shaders");
        fs::create_directories(base / "game" / "build" / "bin" / "Debug");
        fs::create_directories(base / "other" / "assets" / "shaders");
        fs::create_directories(base / "empty" / "deep" / "er");
        fs::create_directories(base / "notadir" / "assets");
        std::ofstream(base / "notadir" / "assets" / "shaders") << "a file";
    }
    ~Tree() { std::error_code ec; fs::remove_all(base, ec); }
    fs::path base;
};

static bool Same(const std::optional<fs::path>& found, const fs::path& expected)
{
    return found && fs::equivalent(*found, expected);
}

static void TestFind()
{
    Tree t;
    CHECK(ProjectRoot::IsRoot(t.base / "game") && !ProjectRoot::IsRoot(t.base / "game" / "build") && !ProjectRoot::IsRoot(t.base / "notadir"));

    CHECK(Same(ProjectRoot::Find({ t.base / "game" }), t.base / "game"));                                // the start itself
    CHECK(Same(ProjectRoot::Find({ t.base / "game" / "build" / "bin" }), t.base / "game"));              // a parent's parent
    CHECK(Same(ProjectRoot::Find({ t.base / "game" / "build" / "bin" / "Debug" }), t.base / "game"));
    CHECK(Same(ProjectRoot::Find({ (t.base / "game" / "build" / "bin").string() + "/" }), t.base / "game"));   // a trailing slash is the same folder

    // several starts: the first one that leads to a root wins (the executable's folder before the working directory)
    CHECK(Same(ProjectRoot::Find({ t.base / "empty" / "deep" / "er", t.base / "other" }), t.base / "other"));
    CHECK(Same(ProjectRoot::Find({ t.base / "game", t.base / "other" }), t.base / "game"));
    CHECK(Same(ProjectRoot::Find({ t.base / "no_such_folder", t.base / "other" }), t.base / "other"));   // a start that does not exist is skipped

    // nothing found
    CHECK(!ProjectRoot::Find({ t.base / "empty" / "deep" / "er" }, 2));
    CHECK(!ProjectRoot::Find({ t.base / "notadir" }, 0));                                                // a FILE named shaders is not a folder
    CHECK(!ProjectRoot::Find({}));
    CHECK(!ProjectRoot::Find({ t.base / "no_such_folder" }));

    // the number of levels is a limit: Debug -> bin -> build -> game is three steps up
    CHECK(!ProjectRoot::Find({ t.base / "game" / "build" / "bin" / "Debug" }, 2));
    CHECK(Same(ProjectRoot::Find({ t.base / "game" / "build" / "bin" / "Debug" }, 3), t.base / "game"));

    // the answer is absolute, even for a relative start
    const fs::path old = fs::current_path();
    fs::current_path(t.base / "game" / "build");
    const auto rel = ProjectRoot::Find({ fs::path("bin") });
    fs::current_path(old);
    CHECK(rel && rel->is_absolute() && Same(rel, t.base / "game"));
}

static void TestEnter()
{
    Tree t;
    const fs::path old = fs::current_path();

    const auto root = ProjectRoot::Enter({ t.base / "game" / "build" / "bin" });
    CHECK(Same(root, t.base / "game") && fs::equivalent(fs::current_path(), t.base / "game"));
    CHECK(fs::is_directory(fs::path("assets") / "shaders"));                                              // relative asset paths now mean something

    fs::current_path(t.base / "empty");
    CHECK(!ProjectRoot::Enter({ t.base / "empty" / "deep" }, 1));
    CHECK(fs::equivalent(fs::current_path(), t.base / "empty"));                                          // nothing found: the folder is as it was

    fs::current_path(old);
}

int main()
{
    struct Test { const char* name; void (*fn)(); };
    const Test tests[] = {
        { "find: levels, several starts, nothing", TestFind },
        { "enter: the working directory",          TestEnter },
    };
    for (const Test& t : tests)
    {
        const int before = g_failed;
        t.fn();
        std::printf("%s  %s\n", g_failed == before ? "PASS" : "FAIL", t.name);
    }
    std::printf(g_failed ? "\n%d check(s) FAILED\n" : "\nAll project root tests passed\n", g_failed);
    return g_failed ? 1 : 0;
}

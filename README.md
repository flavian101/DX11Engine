# DX11Engine

A Direct3D 11 engine in C++ with an ImGui scene editor. Windows only.

What is in it today: an EnTT scene with a transform hierarchy, clustered forward lighting (thousands of point and spot
lights), cascaded / spot / point shadows in one atlas, a material system with PBR-style shading and a linear colour
workflow, model import through Assimp (including skeletal animation), and an editor with hierarchy, inspector, viewport,
transform gizmo and renderer panels.

What it is not yet: a game runtime. Scenes are built in C++ and cannot be saved, there is no play-mode snapshot and no
HDR pipeline. [docs/ROADMAP.md](docs/ROADMAP.md) lists what is wrong and what comes next.

## Build

Requirements: Windows, Visual Studio with an MSVC that supports C++23 (the `.vcxproj` files use toolset v145), CMake 3.20
or newer (3.26+ recommended: the Editor's asset copy is then incremental), Git.

```bash
git clone https://github.com/flavian101/DX11Engine.git
cd DX11Engine
git submodule update --init --recursive    # ImGui follows the 'docking' branch of the fork (see .gitmodules)
cmake -S . -B build -A x64                  # picks the newest Visual Studio; add -G "<generator>" to choose
cmake --build build --config Release --target Editor
```

You can also open the folder in Visual Studio ("Open a local folder") and let it configure CMake. After pulling a commit
that moves a submodule, run the `git submodule update` line again.

The repository also contains `DXEngine.vcxproj` and `Editor.vcxproj` for Visual Studio users. They are maintained by hand
and there is no `.sln` in the repository; CMake is the supported build.

The Editor's post-build step copies `Editor/assets` (textures, models, **shaders**) next to the executable. Assets and
shaders are found through paths relative to the **working directory** (`assets/shaders/`, `assets/textures/`), so start
the Editor from the folder that contains the executable. Visual Studio's debugger already does (its working directory
is set to that folder). Editing a shader in `Editor/assets/shaders` needs a build (to copy it) and a restart.

## Using the editor

* Hold the **middle mouse button** over the viewport to look around, **W A S D** to fly, the mouse wheel changes the speed.
* Left click picks, **F** frames the selection, **Q W E R** select / move / rotate / scale, **Ctrl+D** duplicates,
  **Del** deletes. **F1** switches between editing and playing (play hides the panels).
* Models are imported from the Assets panel.
* **File > Save / Open / New** (Ctrl+S / Ctrl+O / Ctrl+N, Ctrl+Shift+S for Save As) write and read the scene as a `.dxscene` text file. It stores
  names for meshes, models and textures, not the assets themselves, so keep the files it names where they were. Problems found while loading
  are listed in the Assets panel's log.
* **View > View 2** shows the scene through a second camera in its own window (it starts at the editor camera's pose;
  "Pin to editor camera" copies the pose again).

## Tests

Most of the engine's logic is tested without a GPU or a window:

```bash
cmake -S . -B build -A x64 -DDXENGINE_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

## Layout

| Path | What |
|---|---|
| `DXEngine/src/RHI` | graphics device interface and the D3D11 backend |
| `DXEngine/src/renderer` | frame context, render graph, passes, systems (extraction, lights, shadows), shader and resource caches |
| `DXEngine/src/scene` | scene, entities, components, transform and animation systems |
| `DXEngine/src/utils` | meshes, materials, texture slots, image loading |
| `DXEngine/src/assets`, `models` | model import and the asset catalog |
| `DXEngine/src/editor` | editor panels, GPU-free editor logic and the ImGui layer: the `DXEngineEditor` library (the engine, `DXEngineCore`, does not link ImGui) |
| `Editor` | the editor application and its assets (`Editor/assets/shaders` holds the HLSL) |
| `Game` | the smallest application on the engine: a lit cube, no UI, links `DXEngineCore` only (CMake only) |
| `DXEngine/tests` | tests |

## Contributing

After cloning, run once: `git config core.hooksPath tools/hooks`. The pre-commit hook refuses new or changed files above
10 MiB (the repository already carries ~450 MB of sample assets and every blob stays in the history; see C6 in the roadmap).

Fork, branch, keep commits small and focused, and say what you tested. Read [docs/ROADMAP.md](docs/ROADMAP.md) first: it
holds the rules the code follows and the list of known problems. If a change makes that file untrue, update it in the
same commit.

## License

See [LICENSE.txt](LICENSE.txt).

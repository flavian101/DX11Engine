#pragma once
#include "ModelImporter.h"
#include <functional>
#include <memory>

namespace DXEngine
{
    namespace RHI { class IGraphicsDevice; }

    // The ModelImporter's file reader backed by the engine's ModelLoader. The loader (which makes GPU meshes and textures)
    // is created on the first import with the device 'deviceProvider' returns then; a null device fails that import with
    // a message instead of crashing. The reader keeps the loader alive, so its cache survives between imports.
    ModelImporter::LoadFunction MakeModelLoaderFunction(std::function<std::shared_ptr<RHI::IGraphicsDevice>()> deviceProvider);
}

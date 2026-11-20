#include "dxpch.h"
#include "ModelLoaderAdapter.h"
#include "models/ModelAsset.h"
#include "models/ModelLoader.h"

namespace DXEngine
{
    ModelImporter::LoadFunction MakeModelLoaderFunction(std::function<std::shared_ptr<RHI::IGraphicsDevice>()> deviceProvider)
    {
        auto loader = std::make_shared<std::unique_ptr<ModelLoader>>();      // shared by the copies of the returned function
        return [loader, deviceProvider](const std::string& path, bool reload, std::string& error) -> std::shared_ptr<ModelAsset>
        {
            if (!*loader)
            {
                std::shared_ptr<RHI::IGraphicsDevice> device = deviceProvider ? deviceProvider() : nullptr;
                if (!device)
                {
                    error = "there is no graphics device";
                    return nullptr;
                }
                *loader = std::make_unique<ModelLoader>(device);
            }
            if (reload) (*loader)->ClearCache();                    // a reimport must read the file again
            std::shared_ptr<ModelAsset> asset = (*loader)->LoadModel(path);
            if (!asset) error = (*loader)->GetLastError();
            return asset;
        };
    }
}

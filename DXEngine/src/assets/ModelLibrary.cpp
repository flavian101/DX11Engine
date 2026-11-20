#include "dxpch.h"
#include "ModelLibrary.h"
#include "AssetCatalog.h"
#include "models/ModelAsset.h"
#include <algorithm>

namespace DXEngine
{
    ImportedModel& ModelLibrary::Add(const std::string& path, std::shared_ptr<ModelAsset> asset, double importMs)
    {
        const std::string key = Assets::NormalizePath(path);
        auto it = std::find_if(m_Models.begin(), m_Models.end(), [&](const ImportedModel& m) { return m.key == key; });
        if (it == m_Models.end())
        {
            m_Models.emplace_back();
            it = m_Models.end() - 1;
            it->key = key;
        }
        it->path = path;
        it->name = Assets::FileStem(path);
        it->asset = std::move(asset);
        it->importMs = importMs;
        return *it;
    }

    const ImportedModel* ModelLibrary::Find(const std::string& path) const
    {
        const std::string key = Assets::NormalizePath(path);
        auto it = std::find_if(m_Models.begin(), m_Models.end(), [&](const ImportedModel& m) { return m.key == key; });
        return it == m_Models.end() ? nullptr : &*it;
    }

    bool ModelLibrary::Remove(const std::string& path)
    {
        const std::string key = Assets::NormalizePath(path);
        auto it = std::remove_if(m_Models.begin(), m_Models.end(), [&](const ImportedModel& m) { return m.key == key; });
        const bool removed = it != m_Models.end();
        m_Models.erase(it, m_Models.end());
        return removed;
    }

    void ModelLibrary::NoteInstance(const std::string& path)
    {
        const std::string key = Assets::NormalizePath(path);
        for (ImportedModel& m : m_Models)
            if (m.key == key) { ++m.instances; return; }
    }
}

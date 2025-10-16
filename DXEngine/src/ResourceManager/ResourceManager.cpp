#include "dxpch.h"
#include "ResourceManager.h"

namespace DXEngine
{
	template<typename T>
	ResourceManager<T>::ResourceManager(LoaderFunc loader, const std::string& typeName)
		:
		m_Loader(loader),
		m_TypeName(typeName),
		m_NextHandle(1)
	{
		static_assert(std::is_base_of_v<IResource, T>, "T must derive from IResource");
	}
	template<typename T>
	ResourceManager<T>::~ResourceManager()
	{
		UnLoadAll();
	}
	template<typename T>
	ResourceHandle ResourceManager<T>::Load(const std::string& path)
	{
		std::lock_guard<std::mutex> lock(m_Mutex);

		//check if already loaded
		auto it = m_PathToHandle.find(path);
		if (it != m_PathToHandle.end())
		{
			auto& entry = m_Resources[it->second];
			if (entry.state == ResourceState::Loaded)
			{
				return it->second;
			}
		}

		//load New Resource
		auto resource = m_Loader(path);
		if (!resource)
		{
			OutputDebugStringA((m_TypeName + ": Failed to load " + path + "\n").c_str());
			return INVALID_RESOURCE_HANDLE;
		}

		ResourceHandle handle = m_NextHandle++;
		resource->SetHandle(handle);

		ResourceEntry entry;
		entry.resource = resource;
		entry.path = path;
		entry.state = ResourceState::Loaded;
		entry.refCount = 1;

		m_Resources[handle] = entry;
		m_PathToHandle[path] = handle;

		OutputDebugStringA((m_TypeName + ": Loaded " + path +
			" (Handle: " + std::to_string(handle) + ")\n").c_str());

		return handle;
	}
	template<typename T>
	ResourceHandle ResourceManager<T>::LoadAsync(const std::string& path, CompletionCallback onComplete)
	{
		std::lock_guard<std::mutex> lock(m_Mutex);

		// Check if already loaded or loading
		auto it = m_PathToHandle.find(path);
		if (it != m_PathToHandle.end()) {
			auto& entry = m_Resources[it->second];
			if (entry.state == ResourceState::Loaded && onComplete) {
				onComplete(it->second, entry.resource);
			}
			return it->second;
		}

		// Create placeholder entry
		ResourceHandle handle = m_NextHandle++;
		ResourceEntry entry;
		entry.path = path;
		entry.state = ResourceState::Loading;
		entry.refCount = 1;

		m_Resources[handle] = entry;
		m_PathToHandle[path] = handle;

		// Queue async load (job system required)
		// For now, load synchronously
		auto resource = m_Loader(path);

		{
			std::lock_guard<std::mutex> asyncLock(m_Mutex);
			auto& asyncEntry = m_Resources[handle];

			if (resource) {
				resource->SetHandle(handle);
				asyncEntry.resource = resource;
				asyncEntry.state = ResourceState::Loaded;

				if (onComplete) {
					onComplete(handle, resource);
				}
			}
			else {
				asyncEntry.state = ResourceState::Failed;
			}
		}

		return handle;
	}
	template<typename T>
	std::shared_ptr<T> ResourceManager<T>::Get(ResourceHandle handle)
	{
		std::lock_guard<std::mutex> lock(m_Mutex);
		auto it = m_Resources.find(handle);

		return (it != m_Resources.end() && it->second.state == ResourceState::Loaded)
			? it->second.resource
			: nullptr;
	}
	template<typename T>
	std::shared_ptr<T> ResourceManager<T>::GetByPath(const std::string& path)
	{
		std::lock_guard<std::mutex> lock(m_Mutex);
		auto it = m_PathToHandle.find(path);
		if (it != m_PathToHandle.end()) {
			return Get(it->second);
		}
		return nullptr;
	}
	template<typename T>
	ResourceHandle ResourceManager<T>::GetHandle(const std::string& path)
	{
		std::lock_guard<std::mutex> lock(m_Mutex);
		auto it = m_PathToHandle.find(path);
		return it != m_PathToHandle.end() ? it->second : INVALID_RESOURCE_HANDLE;
	}
	template<typename T>
	void ResourceManager<T>::AddRef(ResourceHandle Handle)
	{
		td::lock_guard<std::mutex> lock(m_Mutex);
		auto it = m_Resources.find(handle);
		if (it != m_Resources.end()) {
			it->second.refCount++;
		}
	}
	template<typename T>
	void ResourceManager<T>::Release(ResourceHandle handle)
	{
		std::lock_guard<std::mutex> lock(m_Mutex);
		auto it = m_Resources.find(handle);
		if (it != m_Resources.end()) {
			it->second.refCount--;
			if (it->second.refCount <= 0) {
				Unload(handle);
			}
		}
	}
	template<typename T>
	void ResourceManager<T>::UnLoad(ResourceHandle handle)
	{
		std::lock_guard<std::mutex> lock(m_Mutex);
		auto it = m_Resources.find(handle);
		if (it != m_Resources.end()) {
			m_PathToHandle.erase(it->second.path);
			m_Resources.erase(it);

			OutputDebugStringA((m_TypeName + ": Unloaded resource (Handle: " +
				std::to_string(handle) + ")\n").c_str());
		}
	}
	template<typename T>
	void ResourceManager<T>::UnLoadAll()
	{
		std::lock_guard<std::mutex> lock(m_Mutex);
		m_Resources.clear();
		m_PathToHandle.clear();
		OutputDebugStringA((m_TypeName + ": Unloaded all resources\n").c_str());
	}
	template<typename T>
	void ResourceManager<T>::UnLoadUnused()
	{
		std::lock_guard<std::mutex> lock(m_Mutex);
		std::vector<ResourceHandle> toUnload;

		for (const auto& [handle, entry] : m_Resources) {
			if (entry.refCount <= 1) { // Only manager holds reference
				toUnload.push_back(handle);
			}
		}

		for (ResourceHandle handle : toUnload) {
			m_PathToHandle.erase(m_Resources[handle].path);
			m_Resources.erase(handle);
		}

		if (!toUnload.empty()) {
			OutputDebugStringA((m_TypeName + ": Unloaded " +
				std::to_string(toUnload.size()) + " unused resources\n").c_str());
		}
	}
	template<typename T>
	void ResourceManager<T>::EnableHotReload(bool enable)
	{
		m_HotReloadEnabled = enable;

	}
	template<typename T>
	void ResourceManager<T>::checkForChanges()
	{
		if (!m_HotReloadEnabled) return;

		std::lock_guard<std::mutex> lock(m_Mutex);

		for (auto& [handle, entry] : m_Resources) {
			if (entry.state == ResourceState::Loaded) {
				// Check file modification time (implement based on your needs)
				// For now, just a placeholder
			}
		}
	}
	template<typename T>
	void ResourceManager<T>::Reload(ResourceHandle handle)
	{
		std::lock_guard<std::mutex> lock(m_Mutex);
		auto it = m_Resources.find(handle);
		if (it != m_Resources.end()) {
			auto newResource = m_Loader(it->second.path);
			if (newResource) {
				newResource->SetHandle(handle);
				it->second.resource = newResource;
				it->second.state = ResourceState::Loaded;

				OutputDebugStringA((m_TypeName + ": Reloaded " +
					it->second.path + "\n").c_str());
			}
		}
	}
	template<typename T>
	size_t ResourceManager<T>::GetLoadedResourceCount() const
	{
		std::lock_guard<std::mutex> lock(m_Mutex);
		return m_Resources.size();
	}
	template<typename T>
	size_t ResourceManager<T>::GetTotalMemoryUsage() const
	{
		std::lock_guard<std::mutex> lock(m_Mutex);
		size_t total = 0;
		for (const auto& [handle, entry] : m_Resources) {
			if (entry.resource && entry.state == ResourceState::Loaded) {
				total += entry.resource->GetMemoryUsage();
			}
		}
		return total;
	}
	template<typename T>
	std::vector<std::string> ResourceManager<T>::GetLoadedPaths() const
	{
		std::lock_guard<std::mutex> lock(m_Mutex);
		std::vector<std::string> paths;
		paths.reserve(m_Resources.size());
		for (const auto& [handle, entry] : m_Resources) {
			paths.push_back(entry.path);
		}
		return paths;
	}
	template<typename T>
	std::string ResourceManager<T>::GetDebugInfo() const
	{
		std::lock_guard<std::mutex> lock(m_Mutex);

		std::string info = "=== " + m_TypeName + " Manager ===\n";
		info += "Total Resources: " + std::to_string(m_Resources.size()) + "\n";
		info += "Memory Usage: " + std::to_string(GetTotalMemoryUsage() / 1024) + " KB\n";
		info += "Hot Reload: " + std::string(m_HotReloadEnabled ? "ON" : "OFF") + "\n";
		info += "\nLoaded Resources:\n";

		for (const auto& [handle, entry] : m_Resources) {
			info += "  [" + std::to_string(handle) + "] " + entry.path;
			info += " (RefCount: " + std::to_string(entry.refCount) + ")";
			info += " (State: " + StateToString(entry.state) + ")\n";
		}

		return info;
	}
}
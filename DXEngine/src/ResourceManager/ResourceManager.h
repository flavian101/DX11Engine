#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <atomic>
#include <vector>

namespace DXEngine
{
	//Resource Handle System
	using ResourceHandle = uint64_t;
	constexpr ResourceHandle  INVALID_RESOURCE_HANDLE = 0;

	enum class ResourceState
	{
		Unloaded,
		Loading,
		Loaded,
		Failed
	};

	//Base interface for all managed Resources

	class IResource
	{
	public:
		virtual ~IResource() = default;
		
		virtual ResourceHandle GetHandle() const = 0;
		virtual const std::string& GetPath() const = 0;
		virtual ResourceState GetState() const = 0;
		virtual size_t GetMemoryUsage() const = 0;
		virtual bool IsValid() const = 0;
		virtual void SetHandle(ResourceHandle handle) = 0;
	};

	//BaseResource - CRTP(Curiously Recurring Template Pattern) for resource types

	template<typename Derived>
	class BaseResource : public IResource
	{
	public:
		ResourceHandle Gethandle()const override { return m_Handle; }
		const std::string& GetPath() const override { return m_Path; }
		ResourceState GetState() const override { return m_State; }
		void SetHandle(ResourceHandle handle) override { m_Handle = handle; }


		//derived Classes should implement these
		bool IsValid() const override { return m_State == ResourceState::Loaded; }
		size_t GetMemoryUsage() const override { return sizeof(Derived); }

	protected:
		ResourceHandle m_Handle = INVALID_RESOURCE_HANDLE;
		std::string m_Path;
		ResourceState m_State = ResourceState::Unloaded;
	};


	template<typename T>
	class ResourceManager
	{
	public:
		// Loader function signature: takes path, returns resource
		using LoaderFunc = std::function<std::shared_ptr<T>(const std::string&)>;
		//Async completion Callback
		using CompletionCallback = std::function<void(ResourceHandle, std::shared_ptr<T>)>;

		explicit ResourceManager(LoaderFunc loader, const std::string& typeName = "Resource");
		~ResourceManager();

		//Syncronous Loading
		ResourceHandle Load(const std::string& path);
		//Asynchronous Loading
		ResourceHandle LoadAsync(const std::string& path, CompletionCallback onComplete = nullptr);

		//resource Access
		std::shared_ptr<T> Get(ResourceHandle handle);
		std::shared_ptr<T> GetByPath(const std::string& path);
		ResourceHandle GetHandle(const std::string& path);

		//Reference Counting
		void AddRef(ResourceHandle Handle);
		void Release(ResourceHandle handle);

		//Resource Managment
		void UnLoad(ResourceHandle handle);
		void UnLoadAll();
		void UnLoadUnused();

		//Hot Reloading
		void EnableHotReload(bool enable);
		void checkForChanges();
		void Reload(ResourceHandle handle);

		//statistics
		size_t GetLoadedResourceCount()const;
		size_t GetTotalMemoryUsage()const;
		std::vector<std::string> GetLoadedPaths() const;
		std::string GetDebugInfo() const;
		
	private:
		struct ResourceEntry
		{
			std::shared_ptr<T> resource;
			std::string path;
			ResourceState state = ResourceState::Unloaded;
			int32_t refCount = 0;
		};

		static std::string StateToString(ResourceState state) {
			switch (state) {
			case ResourceState::Unloaded: return "Unloaded";
			case ResourceState::Loading: return "Loading";
			case ResourceState::Loaded: return "Loaded";
			case ResourceState::Failed: return "Failed";
			default: return "Unknown";
			}
		}

		LoaderFunc m_Loader;
		std::string m_TypeName;

		std::unordered_map<ResourceHandle, ResourceEntry> m_Resources;
		std::unordered_map<std::string, ResourceHandle> m_PathToHandle;

		mutable std::mutex m_Mutex;
		std::atomic<ResourceHandle> m_NextHandle;
		bool m_HotReloadEnabled = false;
	};

	// ============================================================================
   // Resource Handle Wrapper (RAII)
   // ============================================================================

	template<typename T>
	class ResourcePtr {
	public:
		ResourcePtr() = default;

		ResourcePtr(ResourceManager<T>* manager, ResourceHandle handle)
			: m_Manager(manager), m_Handle(handle) {
			if (m_Manager && m_Handle != INVALID_RESOURCE_HANDLE) {
				m_Manager->AddRef(m_Handle);
			}
		}

		~ResourcePtr() {
			Release();
		}

		// Copy
		ResourcePtr(const ResourcePtr& other)
			: m_Manager(other.m_Manager), m_Handle(other.m_Handle) {
			if (m_Manager && m_Handle != INVALID_RESOURCE_HANDLE) {
				m_Manager->AddRef(m_Handle);
			}
		}

		ResourcePtr& operator=(const ResourcePtr& other) {
			if (this != &other) {
				Release();
				m_Manager = other.m_Manager;
				m_Handle = other.m_Handle;
				if (m_Manager && m_Handle != INVALID_RESOURCE_HANDLE) {
					m_Manager->AddRef(m_Handle);
				}
			}
			return *this;
		}

		// Move
		ResourcePtr(ResourcePtr&& other) noexcept
			: m_Manager(other.m_Manager), m_Handle(other.m_Handle) {
			other.m_Manager = nullptr;
			other.m_Handle = INVALID_RESOURCE_HANDLE;
		}

		ResourcePtr& operator=(ResourcePtr&& other) noexcept {
			if (this != &other) {
				Release();
				m_Manager = other.m_Manager;
				m_Handle = other.m_Handle;
				other.m_Manager = nullptr;
				other.m_Handle = INVALID_RESOURCE_HANDLE;
			}
			return *this;
		}

		// Access
		std::shared_ptr<T> Get() const {
			return m_Manager ? m_Manager->Get(m_Handle) : nullptr;
		}

		std::shared_ptr<T> operator->() const {
			return Get();
		}

		bool IsValid() const {
			auto res = Get();
			return res && res->IsValid();
		}

		ResourceHandle GetHandle() const {
			return m_Handle;
		}

		void Release() {
			if (m_Manager && m_Handle != INVALID_RESOURCE_HANDLE) {
				m_Manager->Release(m_Handle);
			}
			m_Manager = nullptr;
			m_Handle = INVALID_RESOURCE_HANDLE;
		}

	private:
		ResourceManager<T>* m_Manager = nullptr;
		ResourceHandle m_Handle = INVALID_RESOURCE_HANDLE;
	};
}




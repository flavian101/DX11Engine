workspace "DX11Engine"
	architecture "x64"

	configurations
	{
		"Debug",
		"Release"
	}
	startproject "Sandbox"


outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

--include directories relative to root folder (solution directory)

project "DXEngine"
	location "DXEngine"
	kind "StaticLib"
	staticruntime "on"
	language "C++"
	cppdialect "C++23"


	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")
	
	pchheader("dxpch.h")
	pchsource("DXEngine/src/dxpch.cpp")


	files{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp",
	}
	includedirs{
		"%{prj.name}/src",
		"%{prj.name}/vendor/stb",
	}
	defines{
		"_CRT_SECURE_NO_WARNINGS",
		"_DEBUG"
	}
	links{
	}

	filter "system:windows"
		systemversion "latest"
		buildoptions { "/utf-8" }

		defines
		{
		}

			filter "configurations:Debug"
				defines "DX_DEBUG"
				runtime "debug"
				symbols "on"

			filter "configurations:Release"
				defines "DX_RELEASE"
				runtime "release"
				optimize "on"

			filter "configurations:Dist"
				defines "DX_DIST"
				runtime "release"
				optimize "On"



project "Sandbox"
location "Sandbox"
	kind "ConsoleApp"
	staticruntime "on"
	language "C++"
	cppdialect "C++23"


	targetdir ("bin/"..outputdir.."/%{prj.name}")
	objdir ("bin-int/"..outputdir.."/%{prj.name}")

	files{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp"
	}
	includedirs{
		"DXEngine/src",
		"DXEngine/vendor/"
	}

	links{
		"DXEngine"
	}

	filter "system:windows"
		systemversion "latest"
		buildoptions { "/utf-8" }

		defines{
		}

	filter "configurations:Debug"
			defines "DX_DEBUG"
			runtime "debug"
			symbols "on"
	filter "configurations:Release"
			defines "DX_RELEASE"
			runtime "release"
			optimize "on"
	filter "configurations:Dist"
			defines "DX_DIST"
			runtime "release"
			optimize "on"



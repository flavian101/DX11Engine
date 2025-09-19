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
Includedir = {}
Includedir["AssimpPublic"] = "DXEngine/vendor/assimp/include"
Includedir["AssimpGen"]    = "DXEngine/vendor/assimp/build/include"


-- Define Assimp paths
AssimpLibPath = "DXEngine/vendor/assimp/build/lib"
AssimpBinPath = "DXEngine/vendor/assimp/build/bin"

-- Add post-build command helper function
function CopyDLL(dllName)
    if os.host() == "windows" then
        postbuildcommands {
            '{COPY} "../%{AssimpBinPath}/%{cfg.buildcfg}/%{dllName}" "%{cfg.targetdir}/"'
        }
    end
end

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
		"%{Includedir.AssimpPublic}",
        "%{Includedir.AssimpGen}",
	}
	defines{
		"_CRT_SECURE_NO_WARNINGS",
		"_DEBUG",
		"ASSIMP_BUILD_STATIC",
	}
	links{
	}

	filter "system:windows"
		systemversion "latest"
		buildoptions { "/utf-8" }

		filter "configurations:Debug"
			defines "DX_DEBUG"
			runtime "Debug"
			symbols "on"
			libdirs { "%{AssimpLibPath}/Debug" }
			links   { "assimp-vc143-mtd" }
			CopyDLL("assimp-vc143-mtd.dll")


		filter "configurations:Release"
			defines "DX_RELEASE"
			runtime "Release"
			optimize "on"
			libdirs { "%{AssimpLibPath}/Release" }
			links   { "assimp-vc143-mt" }
			CopyDLL("assimp-vc143-mt.dll")

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
		"%{prj.name}/src/**.cpp",
	}
	includedirs{
		"DXEngine/src",
		"DXEngine/vendor/",
		"%{Includedir.AssimpPublic}",
        "%{Includedir.AssimpGen}",
	}

	links{
		"DXEngine"
	}

	filter "system:windows"
		systemversion "latest"
		buildoptions { "/utf-8" }

	filter "configurations:Debug"
			defines "DX_DEBUG"
			runtime "Debug"
			symbols "on"
			libdirs { "%{AssimpLibPath}/Debug" }
			links   { "assimp-vc143-mtd" }
			CopyDLL("assimp-vc143-mtd.dll")

	filter "configurations:Release"
			defines "DX_RELEASE"
			runtime "Release"
			optimize "on"
			libdirs { "%{AssimpLibPath}/Release" }
			links   { "assimp-vc143-mt" }
			CopyDLL("assimp-vc143-mt.dll")




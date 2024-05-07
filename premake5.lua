workspace "VulkanGameEngine"
    architecture "x64"

    configurations
    {
        "Debug",
        "Release"
    }

outputDir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
vulkanSDKDir = "C:/VulkanSDK/1.3.261.1"

project "VulkanGameEngine"
    kind "ConsoleApp"
    language "C++"

    targetdir ("bin/" .. outputDir .. "%{prj.name}")
    objdir ("bin-int/" .. outputDir .. "%{prj.name}")

    files
    {
        "src/**.h",
        "src/**.cpp"
    }

    includedirs
    {
        vulkanSDKDir .. "/Include",
        "External/Include"
    }

    libdirs
    {
        vulkanSDKDir .. "/Lib",
        "External/Libraries"
    }

    links
    {
        "vulkan-1",
        "glfw3"
    }

    prebuildcommands
    {
        "call CompileShaders.bat"
    }

    filter "system:windows"
        cppdialect "C++17"
        staticruntime "Off"
        systemversion "latest"

    filter "configurations:Debug"
        symbols "On"
    
    filter "configurations:Release"
        optimize "On"

    filter {"system:windows", "configurations:Debug"}
        runtime "Debug"

    filter {"system:windows", "configurations:Release"}
        runtime "Release"
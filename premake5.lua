newaction {
    trigger     = "clean",
    description = "Cleans the project: removes bin and bin-int directories",
    execute     = function ()
        print("Removing build artifacts...")
        os.rmdir("./bin")
        os.rmdir("./bin-int")
        print("Clean complete.")
    end
}

workspace "Symphony"
    architecture "x64"
    startproject "Symphony"
    configurations { "Debug", "Release" }
    flags { "MultiProcessorCompile" }

    outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

    IncludeDir = {}
    IncludeDir["Symphony"] = "src"

    project "Symphony"
        location "."
        kind "StaticLib"
        language "C++"
        cppdialect "C++20"
        staticruntime "on"

        targetdir ("bin/" .. outputdir)
        objdir ("bin-int/" .. outputdir)

        files {
            "%{prj.location}/**.h",
            "%{prj.location}/**.cpp",
            "%{prj.location}/**.hpp"
        }

        includedirs {
            "%{IncludeDir.Symphony}"
        }

        defines { "_CRT_SECURE_NO_WARNINGS" }

        filter "system:windows"
            systemversion "latest"

        filter "configurations:Debug"
            defines { "SYMPHONY_DEBUG" }
            runtime "Debug"
            symbols "on"

        filter "configurations:Release"
            defines { "SYMPHONY_RELEASE" }
            runtime "Release"
            optimize "on"
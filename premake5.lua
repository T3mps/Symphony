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
    -- IncludeDir["GoogleTest"] = "vendor/googletest/include"
    
    group "Dependencies"
        -- include "vendor/googletest"
    
    project "Symphony"
        location "Symphony"
        kind "SharedLib"
        language "C++"
        cppdialect "C++20"
        staticruntime "on"

        targetdir ("bin/" .. outputdir)
        objdir ("bin-int/" .. outputdir)

        files {
            "%{prj.location}/src/**.h",
            "%{prj.location}/src/**.cpp",
            "%{prj.location}/src/**.hpp"
        }    

        includedirs {
            "%{IncludeDir.Symphony}"
        }

        defines { "_CRT_SECURE_NO_WARNINGS" }

        filter "system:windows"
            systemversion "latest"
            defines { "SYMPHONY_BUILD_DLL" }

        filter { "system:windows", "configurations:Release" }
            buildoptions { "/MT" }

        filter "configurations:Debug"
            defines { "SYMPHONY_DEBUG" }
            runtime "Debug"
            symbols "on"

        filter "configurations:Release"
            defines { "SYMPHONY_RELEASE" }
            runtime "Release"
            optimize "on"
        
    project "SymphonyTests"
        location "SymphonyTests"
        kind "ConsoleApp"
        language "C++"
        cppdialect "C++20"
        staticruntime "on"

        targetdir ("bin/" .. outputdir)
        objdir ("bin-int/" .. outputdir)

        files {
            "%{prj.location}/src/**.h",
            "%{prj.location}/src/**.cpp",
            "%{prj.location}/src/**.hpp"
        }    

        includedirs {
            "%{IncludeDir.Symphony}",
            -- "%{IncludeDir.GoogleTest}"
        }

        links {
            "Symphony",
            "gtest"
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
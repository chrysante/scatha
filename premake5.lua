function addCppFiles(dir) 
    files(dir.."/**.h")
    files(dir.."/**.hpp")
    files(dir.."/**.c")
    files(dir.."/**.cc")
    files(dir.."/**.cpp")
end

workspace "scatha"

configurations { "Debug", "Release" }
cppdialect "C++20"

filter "configurations:Debug" 
    symbols "On"
filter "configurations:Release"
    optimize "Speed"
    defines "NDEBUG"
filter {}

flags { "MultiProcessorCompile" }

targetdir("build/bin/%{cfg.longname}")
objdir("build/obj/%{cfg.longname}")

architecture "x86_64"

filter "system:macosx"
    buildoptions { 
        "-Wconversion", 
        "-Wall",
        "-Wpedantic",
        "-Wold-style-cast", 
        "-Wno-sign-compare", 
        "-Wno-unused-parameter",
    }
    xcodebuildsettings { 
        ["INSTALL_PATH"]            = "@executable_path",
        ["LD_RUNPATH_SEARCH_PATHS"] = "@loader_path"
    }
filter {}

------------------------------------------
project "scatha-lib"

kind "SharedLib"

addCppFiles "lib"
addCppFiles "include/scatha"
externalincludedirs { "external/utility", "external/gmp/build/include" }
includedirs { "lib" }
libdirs { "external/gmp/build/lib" }
links "utility"
links "gmp"

--prebuildcommands { "./format-all.sh lib/" }

filter "system:macosx"
buildoptions "-fvisibility=hidden"
filter {}

------------------------------------------
project "scatha"

kind "ConsoleApp"

addCppFiles "app"

externalincludedirs {
    "include",
    "lib",
    "external/utility",
    "external/termfmt"
}

links { "scatha-lib", "utility", "termfmt" }

------------------------------------------
project "scatha-test"

kind "ConsoleApp"
externalincludedirs { 
    ".", 
    "include", 
    "external/utility", 
    "external/Catch"
}

externalincludedirs { "lib" }

--prebuildcommands { "./format-all.sh test/" }

addCppFiles "test"
links { "scatha-lib" } 

------------------------------------------
project "playground"

kind "ConsoleApp"
externalincludedirs { "external/utility" }
includedirs { ".", "lib" }

addCppFiles "playground"
files "playground/**.sc"
links { "scatha", "utility" }

filter { "system:macosx"} 
    defines { "PROJECT_LOCATION=\"../../..\"" } -- use different (maybe less fragile) solution for windows
filter { "system:windows" }
    defines { "PROJECT_LOCATION=R\"($(ProjectDir))\"" }

------------------------------------------
include "external/utility"
include "external/termfmt"

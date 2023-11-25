function addCppFiles(dir) 
    files(dir.."/**.h")
    files(dir.."/**.hpp")
    files(dir.."/**.c")
    files(dir.."/**.cc")
    files(dir.."/**.cpp")
    files(dir.."/**.def")
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

targetdir "build/bin/%{cfg.longname}"
objdir "build/obj/%{cfg.longname}"

architecture "x86_64"

filter "system:macosx"
    buildoptions { 
        "-Wconversion", 
        "-Wall",
        "-Wpedantic",
        "-Wold-style-cast", 
        "-Wno-sign-compare", 
        "-Wno-unused-parameter",
        "-Wmissing-field-initializers",
        "-ftemplate-depth=2048" -- until `dyncast` is refactored
    }
    xcodebuildsettings { 
        ["INSTALL_PATH"]            = "@executable_path",
        ["LD_RUNPATH_SEARCH_PATHS"] = "@loader_path"
    }
filter "system:windows"
    defines "_ITERATOR_DEBUG_LEVEL=0"
filter {}

------------------------------------------
project "scatha" -- Main compiler library
kind "SharedLib"
defines "SC_APIEXPORT"

addCppFiles "lib"
-- Public headers
files {
    "include/scatha/**.h", 
    "include/scatha/**.def" 
}
externalincludedirs { 
    "include",
    "svm/include", -- We need some SVM headers but we don't link it
    "external/utility/include", 
    "external/gmp/build/include", 
    "external/mpfr/build/include",
    "external/termfmt/include",
    "external/graphgen/include",
    "external/APMath/include",
    "external/range-v3/include",
    "external/nlohmann/json/include",
}
includedirs { "lib", "include/scatha" }
links { "apmath", "graphgen", "termfmt" }

filter { "system:macosx" }
buildoptions "-fvisibility=hidden"
filter {}

------------------------------------------
project "scatha-test"
kind "ConsoleApp"
defines "SC_APIIMPORT"

includedirs { "lib", "include/scatha" }

externalincludedirs { 
    ".", 
    "include", 
    "svm/include", 
    "external/utility/include", 
    "external/Catch/src",
    "build/Catch/generated-includes",
    "external/APMath/include",
    "external/range-v3/include",
}

addCppFiles "test"
links { "Catch2", "scatha", "svm-lib", "APMath" } 

------------------------------------------

------------------------------------------
project "playground"
kind "ConsoleApp"
defines "SC_APIIMPORT"

externalincludedirs {
    "include",
    "include/scatha", 
    "svm/include", 
    "runtime/include",
    "external/utility/include",
    "external/termfmt/include",
    "external/APMath/include",
    "external/range-v3/include",
}
includedirs { ".", "lib", "playground", "apmath" }

addCppFiles "playground"
files "playground/**.sc"
links { 
    "scatha", 
    "svm-lib", 
    "termfmt" 
}

filter { "system:macosx"} 
    defines { "PROJECT_LOCATION=\"${PROJECT_DIR}\"" }
filter { "system:windows" }
    defines { "PROJECT_LOCATION=R\"($(ProjectDir))\"" }
filter {}

------------------------------------------

-- Weirdly enough include "scathadb" gives a strange error
include "svm/premake5.lua"
include "scathac/premake5.lua"
include "scathadb/premake5.lua"
include "runtime/premake5.lua"
include "sctool/premake5.lua"

include "external/termfmt/lib.lua"
include "external/graphgen/lib.lua"
include "external/APMath/lib.lua"

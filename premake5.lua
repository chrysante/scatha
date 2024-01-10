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

filter "system:linux or macosx"
    buildoptions { 
        "-Wall",
        "-Wextra",
        "-pedantic"
    }
filter "system:macosx"
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
    "external/libarchive/libarchive",
}
includedirs { "lib", "include/scatha" }
libdirs "build/bin/%{cfg.longname}"
links { "apmath", "graphgen", "termfmt", "utility", "archive" }

filter "system:linux"
    buildoptions "-fPIC"
filter "system:linux or macosx"
    buildoptions "-fvisibility=hidden"
filter {}

------------------------------------------
project "scatha-test"
kind "ConsoleApp"
defines "SC_APIIMPORT"

includedirs { "test", "lib", "include/scatha" }

externalincludedirs { 
    "include", 
    "svm/include", 
    "external/utility/include", 
    "external/Catch/src",
    "build/Catch/generated-includes",
    "external/APMath/include",
    "external/range-v3/include",
    "external/termfmt/include",
}

addCppFiles "test"
libdirs "build/bin/%{cfg.longname}"
links { "Catch2", "scatha", "svm-lib", "APMath", "termfmt" } 
dependson "ffi-testlib"

------------------------------------------
project "scatha-benchmark"
kind "ConsoleApp"
defines "SC_APIIMPORT"

includedirs { "benchmark" }

externalincludedirs { 
    "include", 
    "svm/include", 
    "external/utility/include", 
    "external/Catch/src",
    "build/Catch/generated-includes",
    "external/APMath/include",
    "external/range-v3/include",
    "external/termfmt/include",
}

addCppFiles "benchmark"
libdirs "build/bin/%{cfg.longname}"
links { "Catch2", "Catch2Main", "scatha", "svm-lib" } 

------------------------------------------
project "scatha-fuzz"
kind "ConsoleApp"
defines "SC_APIIMPORT"

includedirs { "lib", "include/scatha" }

externalincludedirs { 
    "include", 
    "external/utility/include", 
    "external/APMath/include",
    "external/range-v3/include",
    "external/termfmt/include",
    "external/cli11/include",
}

addCppFiles "fuzz"
links { "scatha", "APMath", "termfmt" } 

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
files { "playground/**.sc", "playground/**.scir" }
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
include "ffi-testlib/premake5.lua"

include "external/termfmt/lib.lua"
include "external/graphgen/lib.lua"
include "external/APMath/lib.lua"

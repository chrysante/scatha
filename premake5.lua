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
    "external/utility/include", 
    "external/gmp/build/include", 
    "external/mpfr/build/include",
    "external/termfmt/include",
    "external/graphgen/include",
    "external/APMath/include",
    "external/range-v3/include",
}
includedirs { "lib", "include/scatha" }
links { "apmath", "graphgen", "termfmt" }

filter { "system:macosx" }
buildoptions "-fvisibility=hidden"
filter {}

------------------------------------------
project "scathac" -- compiler executable
kind "ConsoleApp"
defines "SC_APIIMPORT"
addCppFiles "scathac"

externalincludedirs {
    "include",
    "external/utility/include",
    "external/termfmt/include",
    "external/cli11/include",
    "external/APMath/include",
    "external/range-v3/include",
}

links { "scatha", "termfmt", "apmath" }

------------------------------------------
project "svm-lib"
kind "StaticLib"

addCppFiles "svm"
removefiles { "svm/CLIParse.*", "svm/main.cc" }
files "include/svm/**"

includedirs { "." }

externalincludedirs {
    "include",
    "external/utility/include",
}

------------------------------------------
project "svm"
kind "ConsoleApp"
files { "svm/CLIParse.*", "svm/main.cc" }

externalincludedirs {
    "include",
    "external/utility/include",
    "external/cli11/include",
    "external/range-v3/include",
}

links "svm-lib"

------------------------------------------
project "svm-test"
kind "ConsoleApp"

includedirs { "svm" }

externalincludedirs {
    "include",
    "external/utility/include",
    "external/Catch",
    "external/range-v3/include",
}

addCppFiles "svm-test"
links "svm-lib"

------------------------------------------
project "scatha-test"
kind "ConsoleApp"
defines "SC_APIIMPORT"

externalincludedirs { 
    ".", 
    "include", 
    "include/scatha", 
    "lib",
    "external/utility/include", 
    "external/Catch",
    "external/APMath/include",
    "external/range-v3/include",
}

addCppFiles "test"
links { "scatha", "svm-lib", "APMath" } 

------------------------------------------
project "runtime"
kind "StaticLib"
defines "SC_APIIMPORT"

externalincludedirs {
    "include",
    "include/scatha",
    "runtime/include",
    "external/utility/include",
    "external/termfmt/include",
    "external/APMath/include",
    "external/range-v3/include",
}
includedirs { ".", "lib" }

files { "runtime/**.h", "runtime/**.cc" }
links { "termfmt" }

------------------------------------------
project "playground"
kind "ConsoleApp"
defines "SC_APIIMPORT"

externalincludedirs {
    "include",
    "include/scatha", 
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
    --"runtime", 
    "termfmt" 
}

filter { "system:macosx"} 
    defines { "PROJECT_LOCATION=\"${PROJECT_DIR}\"" }
filter { "system:windows" }
    defines { "PROJECT_LOCATION=R\"($(ProjectDir))\"" }
filter {}

------------------------------------------

include "passtool"

include "external/termfmt/lib.lua"
include "external/graphgen/lib.lua"
include "external/APMath/lib.lua"

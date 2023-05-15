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
filter {}

------------------------------------------
project "scatha"

kind "SharedLib"

addCppFiles "lib"
externalincludedirs { 
    "include",
    "external/utility/include", 
    "external/gmp/build/include", 
    "external/mpfr/build/include",
    "external/termfmt/include",
    "external/APMath/include",
    "external/range-v3/include",
}
includedirs { "lib" }
links { "apmath", "termfmt" }

filter { "system:macosx", "configurations:Release" }
buildoptions "-fvisibility=hidden"
filter {}

filter "system:macosx"
prebuildcommands {
    "${PROJECT_DIR}/scripts/copy-public-headers.sh"
}
filter {}

------------------------------------------
project "scatha-c"

kind "ConsoleApp"

addCppFiles "scatha-c"

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

filter "system:macosx"
prebuildcommands {
    "${PROJECT_DIR}/scripts/copy-public-headers.sh"
}
filter {}

------------------------------------------
project "svm"

kind "ConsoleApp"

files { "svm/CLIParse.*", "svm/main.cc" }

externalincludedirs {
    "include",
    "external/utility/include",
    "external/cli11/include",
}

links "svm-lib"

------------------------------------------
project "scatha-test"

kind "ConsoleApp"
externalincludedirs { 
    ".", 
    "include", 
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
externalincludedirs {
    "include",
    "runtime/include",
    "external/utility/include",
    "external/termfmt/include",
    "external/APMath/include",
    "external/range-v3/include",
}
includedirs { ".", "lib" }

files { "runtime/**.h", "runtime/**.cc" }
links { "scatha", "svm-lib", "termfmt" }

------------------------------------------
project "playground"

kind "ConsoleApp"
externalincludedirs {
    "include",
    "runtime/include",
    "external/utility/include",
    "external/termfmt/include",
    "external/APMath/include",
    "external/range-v3/include",
}
includedirs { ".", "lib", "playground", "apmath" }

addCppFiles "playground"
files "playground/**.sc"
links { "scatha", "svm-lib", "runtime", "termfmt" }

filter { "system:macosx"} 
    defines { "PROJECT_LOCATION=\"${PROJECT_DIR}\"" }
filter { "system:windows" }
    defines { "PROJECT_LOCATION=R\"($(ProjectDir))\"" }
filter {}

------------------------------------------
include "external/termfmt"
include "external/APMath/lib.lua"

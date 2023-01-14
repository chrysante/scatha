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
    "external/boost/config/include",
    "external/boost/detail/include",
    "external/boost/logic/include"
}
includedirs { "lib" }
libdirs { 
    "external/gmp/build/lib", 
    "external/mpfr/build/lib"
}
links { "utility", "gmp", "mpfr" }

filter "system:macosx"
buildoptions "-fvisibility=hidden"
filter {}

prebuildcommands {
    "${PROJECT_DIR}/scripts/copy-public-headers-unix.sh"
}

------------------------------------------
project "scatha-c"

kind "ConsoleApp"

addCppFiles "scatha-c"

externalincludedirs {
    "include",
    "external/utility/include",
    "external/termfmt/include",
    "external/cli11/include"
}

links { "scatha", "utility", "termfmt" }

------------------------------------------
project "scatha-test"

kind "ConsoleApp"
externalincludedirs { 
    ".", 
    "include", 
    "lib",
    "external/utility/include", 
    "external/Catch"
}

addCppFiles "test"
links { "scatha", "utility" } 

------------------------------------------
project "playground"

kind "ConsoleApp"
externalincludedirs {
    "include",
    "external/utility/include" 
}
includedirs { ".", "lib", "playground" }

addCppFiles "playground"
files "playground/**.sc"
links { "scatha", "utility" }

filter { "system:macosx"} 
    defines { "PROJECT_LOCATION=\"${PROJECT_DIR}\"" }
filter { "system:windows" }
    defines { "PROJECT_LOCATION=R\"($(ProjectDir))\"" }
filter {}

------------------------------------------
include "external/utility/lib.lua"
include "external/termfmt"

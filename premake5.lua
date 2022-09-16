function addCppFiles(dir) 
    files(dir.."/**.h")
    files(dir.."/**.hpp")
    files(dir.."/**.c")
    files(dir.."/**.cc")
    files(dir.."/**.cpp")
end

workspace "scatha"

configurations { "debug", "release" }
cppdialect "C++20"

filter "configurations:debug" 
    symbols "On"
filter "configurations:release"
    optimize "Speed"
    defines "NDEBUG"
filter {}

flags { "ExtraWarnings" }

targetdir("build/bin/%{cfg.longname}")
objdir("build/obj/%{cfg.longname}")

------------------------------------------
project "scatha"

kind "SharedLib"
addCppFiles "lib"
addCppFiles "include/scatha"
sysincludedirs { "include", "external", "external/utility" }
includedirs "lib"
links "utility"

------------------------------------------
project "scatha-test"

kind "ConsoleApp"
sysincludedirs { 
    ".", 
    "include", 
    "external/utility", 
    "external/Catch"
}

sysincludedirs { "lib" }

addCppFiles "test"
links { "scatha" } 

------------------------------------------
project "playground"

kind "ConsoleApp"
sysincludedirs { ".", "include", "external/utility" }

sysincludedirs { "lib" }

addCppFiles "playground"
files "playground/**.sc"
links { "scatha", "utility" }

filter { "system:macosx"} 
    defines { "PROJECT_LOCATION=\"../../..\"" } -- use different (maybe less fragile) solution for windows
filter {}

------------------------------------------
include "external/utility"

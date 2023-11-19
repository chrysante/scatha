project "scathadb" 
kind "ConsoleApp"

externalincludedirs {
    "%{wks.location}/svm/include",
    "%{wks.location}/external/FTXUI/include",
    "%{wks.location}/external/range-v3/include",
    "%{wks.location}/external/utility/include",
    "%{wks.location}/external/nlohmann/json/include",
}

includedirs { "src" }

files { "src/**.h", "src/**.cc" }

links { "svm-lib", "ftxui-component", "ftxui-dom", "ftxui-screen" }

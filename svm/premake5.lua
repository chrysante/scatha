------------------------------------------
project "svm-lib"
kind "StaticLib"

files { "lib/**", "include/svm/**" }

includedirs { "lib" }

externalincludedirs {
    "include",
    "%{wks.location}/external/cli11/include",
    "%{wks.location}/external/range-v3/include",
    "%{wks.location}/external/utility/include",
}

------------------------------------------
project "svm"
kind "ConsoleApp"
files { "src/**" }

externalincludedirs {
    "include",
    "%{wks.location}/external/utility/include",
    "%{wks.location}/external/range-v3/include",
}

links "svm-lib"

------------------------------------------
project "svm-test"
kind "ConsoleApp"

files "test/**"

includedirs { "lib" }

externalincludedirs {
    "include",
    "%{wks.location}/external/utility/include",
    "%{wks.location}/external/Catch",
    "%{wks.location}/external/range-v3/include",
}

links "svm-lib"

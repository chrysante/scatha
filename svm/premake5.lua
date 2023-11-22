------------------------------------------
project "svm-lib"
kind "StaticLib"

files { "lib/**.h", "lib/**.cc", "include/svm/**.h", "include/svm/**.def" }

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
files { "src/**.h", "src/**.cc" }

externalincludedirs {
    "include",
    "%{wks.location}/external/utility/include",
    "%{wks.location}/external/range-v3/include",
}

links "svm-lib"

------------------------------------------
project "svm-test"
kind "ConsoleApp"

files { "test/**.h", "test/**.cc" }

includedirs { "lib" }

externalincludedirs {
    "include",
    "%{wks.location}/external/utility/include",
    "%{wks.location}/external/Catch",
    "%{wks.location}/external/range-v3/include",
}

links "svm-lib"

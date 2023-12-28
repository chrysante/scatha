------------------------------------------
project "svm-lib"
kind "StaticLib"

files { "lib/**.h", "lib/**.cc", "include/svm/**.h", "include/svm/**.def" }

includedirs { "lib", "include/svm" }

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
includedirs { "lib", "include/svm" }

libdirs "%{wks.location}/build/bin/%{cfg.longname}"
links { "svm-lib", "utility" }

------------------------------------------
project "svm-test"
kind "ConsoleApp"

files { "test/**.h", "test/**.cc" }

includedirs { "lib", "include/svm" }

externalincludedirs {
    "include",
    "%{wks.location}/external/utility/include",
    "%{wks.location}/external/range-v3/include",
    "%{wks.location}/external/Catch/src",
    "%{wks.location}/build/Catch/generated-includes",
}

libdirs "%{wks.location}/build/bin/%{cfg.longname}"
links { "Catch2", "Catch2Main", "svm-lib" }

-- Main project ----------------------------------------------------------------
project "runtime"
kind "StaticLib"
defines "SC_APIIMPORT"

externalincludedirs {
    "include",
    "%{wks.location}/include",
    "%{wks.location}/svm/include",
    "%{wks.location}/external/utility/include",
    "%{wks.location}/external/termfmt/include",
    "%{wks.location}/external/APMath/include",
    "%{wks.location}/external/range-v3/include",
}
includedirs { "src", "include/scatha" }

files { "include/scatha/Runtime/**.h", "src/**.cc" }

-- Test project ----------------------------------------------------------------
project "runtime-test"
kind "ConsoleApp"
defines "SC_APIIMPORT"

externalincludedirs {
    "include",
    "%{wks.location}/include",
    "%{wks.location}/svm/include",
    "%{wks.location}/external/utility/include",
    "%{wks.location}/external/range-v3/include",
    "%{wks.location}/external/Catch/src",
    "%{wks.location}/build/Catch/generated-includes",
}

files { "test/**.cc" }

links { "Catch2", "Catch2Main", "scatha", "svm-lib", "runtime" }

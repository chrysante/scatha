project "sctool"
kind "ConsoleApp"
defines "SC_APIIMPORT"

externalincludedirs {
    "%{wks.location}/include",
    "%{wks.location}/svm/include",
    "%{wks.location}/external/utility/include",
    "%{wks.location}/external/termfmt/include",
    "%{wks.location}/external/APMath/include",
    "%{wks.location}/external/range-v3/include",
    "%{wks.location}/external/cli11/include",
}
includedirs { "src" }

files { "src/**.h", "src/**.cc" }
links { "termfmt", "scatha", "svm-lib" }
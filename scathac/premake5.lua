project "scathac" -- compiler executable
kind "ConsoleApp"
defines "SC_APIIMPORT"
files { "src/**.h", "src/**.cc" }

externalincludedirs {
    "%{wks.location}/include",
    "%{wks.location}/svm/include",
    "%{wks.location}/external/utility/include",
    "%{wks.location}/external/termfmt/include",
    "%{wks.location}/external/cli11/include",
    "%{wks.location}/external/APMath/include",
    "%{wks.location}/external/range-v3/include",
}

libdirs "%{wks.location}/build/bin/%{cfg.longname}"
links { "scatha", "termfmt", "apmath" }

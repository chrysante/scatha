project "ffi-testlib"
kind "SharedLib"
includedirs { "src" }
files { "src/*.cc" }

-- Copy the library to userlibs/ffi-testlib-nested
-- We do this to test importing libs via nested library names
postbuildcommands {
  "{MKDIR} %{cfg.targetdir}/userlibs",
  "{COPY} %{cfg.buildtarget.relpath} "
        .."%{cfg.targetdir}/userlibs/"
        .."%{cfg.buildtarget.prefix}%{cfg.buildtarget.basename}-nested%{cfg.buildtarget.extension}"
}

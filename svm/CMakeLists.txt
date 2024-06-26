# libsvm

add_library(libsvm STATIC)
set_target_properties(libsvm PROPERTIES LINKER_LANGUAGE CXX)
SCSetCompilerOptions(libsvm)
set_target_properties(libsvm PROPERTIES OUTPUT_NAME "svm")

target_link_libraries(libsvm
  PUBLIC
    utility
  PRIVATE
    range-v3
    ffi
)

target_include_directories(libsvm
    PUBLIC
      ${CMAKE_CURRENT_BINARY_DIR}/include
      include
    PRIVATE
      include/svm
      lib
)

target_sources(libsvm
  PRIVATE
    include/svm/Builtin.def.h
    include/svm/Builtin.h
    include/svm/Common.h
    include/svm/Errors.def.h
    include/svm/Errors.h
    include/svm/Fwd.h
    include/svm/OpCode.def.h
    include/svm/OpCode.h
    include/svm/Program.h
    include/svm/Util.h
    include/svm/VMData.h
    include/svm/VirtualMachine.h
    include/svm/VirtualMemory.h
    include/svm/VirtualPointer.h
)

source_group(include/svm REGULAR_EXPRESSION "include/svm/*")

target_sources(libsvm
  PRIVATE
    lib/ArithmeticOps.h
    lib/Builtin.cc
    lib/BuiltinInternal.h
    lib/Errors.cc
    lib/Execution.cc
    lib/ExternalFunction.h
    lib/Memory.h
    lib/OpCode.cc
    lib/Program.cc
    lib/Util.cc
    lib/VMImpl.h
    lib/VirtualMachine.cc
    lib/VirtualMemory.cc
    lib/VirtualPointer.cc
)

source_group(lib REGULAR_EXPRESSION "lib/*")

# svm

if(SCATHA_BUILD_EXECUTABLES)

add_executable(svm)
set_target_properties(svm PROPERTIES INSTALL_RPATH "@loader_path")
SCSetCompilerOptions(svm)

target_link_libraries(svm
  PRIVATE
    libsvm
    CLI11::CLI11
)

target_include_directories(svm
  PRIVATE
    src
)

target_sources(svm
  PRIVATE
    src/Main.cc
    src/ParseCLI.h
    src/ParseCLI.cc
)

source_group(src REGULAR_EXPRESSION "src/*")

endif() # SCATHA_BUILD_EXECUTABLES

# svm-test

if(SCATHA_BUILD_TESTS)

add_executable(svm-test)
SCSetCompilerOptions(svm-test)

target_link_libraries(svm-test
  PRIVATE
    libsvm
    Catch2::Catch2WithMain
)

target_include_directories(svm-test
  PRIVATE
    lib
)

target_sources(svm-test
  PRIVATE
    test/VirtualMemory.t.cc
)

source_group(svm-test REGULAR_EXPRESSION "test/*")

endif() # SCATHA_BUILD_TESTS

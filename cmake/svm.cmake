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

set(libsvm_headers
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
    include/svm/VirtualPointer.h)

set(libsvm_sources
    src/svm/ArithmeticOps.h
    src/svm/Builtin.cc
    src/svm/BuiltinInternal.h
    src/svm/Errors.cc
    src/svm/Execution.cc
    src/svm/ExecutionInstDef.h
    src/svm/ExternalFunction.h
    src/svm/Memory.h
    src/svm/OpCode.cc
    src/svm/Program.cc
    src/svm/Util.cc
    src/svm/VMImpl.h
    src/svm/VirtualMachine.cc
    src/svm/VirtualMemory.cc
    src/svm/VirtualPointer.cc
)

target_sources(libsvm
  PRIVATE
    ${libsvm_headers}
    ${libsvm_sources}
)

source_group(TREE ${PROJECT_SOURCE_DIR}/include/svm FILES ${libsvm_headers})
source_group(TREE ${PROJECT_SOURCE_DIR}/src/svm FILES ${libsvm_sources})

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
    src/svm
)

set(svm_sources
    src/svm/Main.cc
    src/svm/ParseCLI.h
    src/svm/ParseCLI.cc
)

target_sources(svm
  PRIVATE
    ${svm_sources}
)

source_group(TREE ${PROJECT_SOURCE_DIR}/src/svm FILES ${svm_sources})

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

set(svm_test_sources
  test/svm/VirtualMemory.t.cc
)

target_sources(svm-test
  PRIVATE
    ${svm_test_sources}
)

source_group(TREE ${PROJECT_SOURCE_DIR}/test/svm FILES ${svm_test_sources})

endif() # SCATHA_BUILD_TESTS

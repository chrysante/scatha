# libsvm

add_library(libsvm)
set_target_properties(libsvm PROPERTIES LINKER_LANGUAGE CXX)

target_link_libraries(libsvm
  PUBLIC
    utility
  PRIVATE
    range-v3
    libffi::libffi
    CLI11::CLI11
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
    include/svm/Builtin.def
    include/svm/Builtin.h
    include/svm/Common.h
    include/svm/Errors.def
    include/svm/Errors.h
    include/svm/Fwd.h
    include/svm/OpCode.def
    include/svm/OpCode.h
    include/svm/ParseCLI.h
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
    lib/ParseCLI.cc
    lib/Program.cc
    lib/Util.cc
    lib/VMImpl.h
    lib/VirtualMachine.cc
    lib/VirtualMemory.cc
    lib/VirtualPointer.cc
)

source_group(lib REGULAR_EXPRESSION "lib/*")

# svm

add_executable(svm)

target_link_libraries(svm
  PRIVATE
    libsvm
)

target_sources(svm
  PRIVATE
    src/Main.cc
)

source_group(src REGULAR_EXPRESSION "src/*")
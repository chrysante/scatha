set(STDLIB_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/stdlib")
set(COMPILE_STDLIB_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/stdlib/compile-stdlib.sh")
set(COMPILED_STDLIB "${STDLIB_OUTPUT_DIR}/std.sclib")

add_custom_command(
  OUTPUT "${COMPILED_STDLIB}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${STDLIB_OUTPUT_DIR}"
  COMMAND "${COMPILE_STDLIB_SCRIPT}" -C "$<TARGET_FILE:scathac>" -D "${STDLIB_OUTPUT_DIR}"
  DEPENDS scathac "${COMPILE_STDLIB_SCRIPT}"
  COMMENT "Building std.sclib"
)

add_custom_target(build_stdlib ALL
  DEPENDS "${COMPILED_STDLIB}"
)
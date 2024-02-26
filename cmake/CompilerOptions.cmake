
function(SCSetCompilerOptions target)
  if (WIN32)
    if (MSVC)
      target_compile_options(${target} PRIVATE "/W3")
      target_compile_options(${target} PRIVATE "/WX")
      target_compile_options(${target} PRIVATE "/D_CRT_SECURE_NO_WARNINGS")
    endif()
  else()
    target_compile_options(${target} PRIVATE "-Wall")
    target_compile_options(${target} PRIVATE "-Wextra")
    target_compile_options(${target} PRIVATE "-pedantic")

    target_compile_options(${target} PRIVATE "-Wdeprecated")
    target_compile_options(${target} PRIVATE "-Wmissing-declarations")
    target_compile_options(${target} PRIVATE "-Wnon-virtual-dtor")
    target_compile_options(${target} PRIVATE "-Wnull-dereference")
    target_compile_options(${target} PRIVATE "-Woverloaded-virtual")
    # We enable this later, this triggers too often
    #target_compile_options(${target} PRIVATE "-Wshadow")
    target_compile_options(${target} PRIVATE "-Wunused")

    target_compile_options(${target} PRIVATE "$<$<CONFIG:Debug>:-fsanitize=address>")
    target_link_options(${target} PRIVATE "$<$<CONFIG:Debug>:-fsanitize=address>")
    target_compile_options(${target} PRIVATE "$<$<CONFIG:Debug>:-fsanitize=undefined>")
    target_link_options(${target} PRIVATE "$<$<CONFIG:Debug>:-fsanitize=undefined>")
  endif()
endfunction()

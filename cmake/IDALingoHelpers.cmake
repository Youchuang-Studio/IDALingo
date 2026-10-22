function(idalingo_enable_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE
      $<$<COMPILE_LANGUAGE:CXX>:/W4>
      $<$<COMPILE_LANGUAGE:CXX>:/permissive->
      $<$<COMPILE_LANGUAGE:CXX>:/utf-8>)
    if(IDALINGO_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/WX>)
    endif()
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    if(IDALINGO_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()
endfunction()

function(idalingo_copy_language target)
  add_custom_target(${target}_language ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${IDALINGO_LANGUAGE_FILE}"
      "$<TARGET_FILE_DIR:${target}>/language.txt"
    DEPENDS ${target}
    VERBATIM)
endfunction()

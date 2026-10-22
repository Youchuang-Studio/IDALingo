enable_language(ASM_MASM)
configure_file(winmm_proxy.asm winmm_proxy.asm COPYONLY)

set(IDALINGO_WINDOWS_SOURCES
  dllmain.cpp
  qt_hooks.cpp
  qt_hooks.hpp
  qt_string_abi.cpp
  qt_string_abi.hpp
  winmm_proxy.cpp
  winmm_proxy.hpp
  "${CMAKE_CURRENT_BINARY_DIR}/winmm_proxy.asm"
  winmm_exports.hpp
  winmm.def
)

function(idalingo_add_windows_proxy target output_directory)
  add_library(${target} SHARED ${IDALINGO_WINDOWS_SOURCES})
  target_compile_features(${target} PRIVATE cxx_std_20)
  target_compile_definitions(${target} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
  target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
  target_link_libraries(${target} PRIVATE idalingo_translation idalingo_text_exporter minhook)
  target_link_options(${target} PRIVATE "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/winmm.def")
  set_target_properties(${target} PROPERTIES
    PREFIX ""
    OUTPUT_NAME "winmm"
    RUNTIME_OUTPUT_DIRECTORY "${output_directory}"
    LIBRARY_OUTPUT_DIRECTORY "${output_directory}"
    ARCHIVE_OUTPUT_DIRECTORY "${output_directory}"
    PDB_OUTPUT_DIRECTORY "${output_directory}")
  idalingo_enable_warnings(${target})
endfunction()

idalingo_add_windows_proxy(idalingo "${CMAKE_BINARY_DIR}/out")
idalingo_copy_language(idalingo)

if(IDALINGO_BUILD_EXPORTER)
  idalingo_add_windows_proxy(idalingo_export "${CMAKE_BINARY_DIR}/export-only")
  target_compile_definitions(idalingo_export PRIVATE IDALINGO_EXPORT_ONLY=1)
  install(TARGETS idalingo_export RUNTIME DESTINATION export-only)
endif()

install(TARGETS idalingo RUNTIME DESTINATION .)

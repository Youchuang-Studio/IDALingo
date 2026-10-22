function(idalingo_add_linux_preload target output_name output_directory)
  add_library(${target} SHARED preload.cpp)
  target_compile_features(${target} PRIVATE cxx_std_20)
  target_compile_definitions(${target} PRIVATE _GNU_SOURCE)
  target_link_libraries(${target} PRIVATE idalingo_translation idalingo_text_exporter dl)
  target_link_options(${target} PRIVATE
    "LINKER:--version-script=${CMAKE_CURRENT_SOURCE_DIR}/qt6_exports.map")
  set_target_properties(${target} PROPERTIES
    OUTPUT_NAME "${output_name}"
    LIBRARY_OUTPUT_DIRECTORY "${output_directory}")
  idalingo_enable_warnings(${target})
endfunction()

idalingo_add_linux_preload(idalingo idalingo "${CMAKE_BINARY_DIR}/out")
idalingo_copy_language(idalingo)

add_executable(idalingo_launcher launcher.cpp)
target_compile_features(idalingo_launcher PRIVATE cxx_std_20)
target_compile_definitions(idalingo_launcher PRIVATE
  IDALINGO_DEFAULT_IDA_EXECUTABLE="${IDALINGO_IDA_EXECUTABLE}")
set_target_properties(idalingo_launcher PROPERTIES
  OUTPUT_NAME "idalingo"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out")
add_dependencies(idalingo_launcher idalingo_language)
idalingo_enable_warnings(idalingo_launcher)

if(IDALINGO_BUILD_EXPORTER)
  idalingo_add_linux_preload(idalingo_export idalingo_export
    "${CMAKE_BINARY_DIR}/export-only")
  target_compile_definitions(idalingo_export PRIVATE IDALINGO_EXPORT_ONLY=1)
  install(TARGETS idalingo_export LIBRARY DESTINATION export-only)
endif()

install(TARGETS idalingo idalingo_launcher
  LIBRARY DESTINATION .
  RUNTIME DESTINATION .)

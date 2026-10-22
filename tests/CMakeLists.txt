add_executable(translation_tests unit/translation_tests.cpp)
target_compile_features(translation_tests PRIVATE cxx_std_20)
target_link_libraries(translation_tests PRIVATE idalingo_translation)
idalingo_enable_warnings(translation_tests)
add_test(NAME translation_tests
  COMMAND translation_tests "${IDALINGO_LANGUAGE_FILE}")

add_executable(text_exporter_tests unit/text_exporter_tests.cpp)
target_compile_features(text_exporter_tests PRIVATE cxx_std_20)
target_link_libraries(text_exporter_tests PRIVATE idalingo_text_exporter)
idalingo_enable_warnings(text_exporter_tests)
add_test(NAME text_exporter_tests COMMAND text_exporter_tests)

if(WIN32)
  set(IDALINGO_WINDOWS_SOURCE_DIR "${PROJECT_SOURCE_DIR}/src/windows")
  add_executable(qt_string_abi_tests
    abi/qt_string_abi_tests.cpp
    "${IDALINGO_WINDOWS_SOURCE_DIR}/qt_string_abi.cpp")
  target_compile_features(qt_string_abi_tests PRIVATE cxx_std_20)
  target_compile_definitions(qt_string_abi_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_include_directories(qt_string_abi_tests PRIVATE "${IDALINGO_WINDOWS_SOURCE_DIR}")
  idalingo_enable_warnings(qt_string_abi_tests)
  add_test(NAME qt_string_abi_tests COMMAND qt_string_abi_tests)
elseif(IDALINGO_QT_DIR AND EXISTS "${IDALINGO_QT_DIR}/libQt6Core.so.6")
  add_executable(qt_abi_probe abi/qt_abi_probe.cpp)
  target_compile_features(qt_abi_probe PRIVATE cxx_std_20)
  target_link_libraries(qt_abi_probe PRIVATE "${IDALINGO_QT_DIR}/libQt6Core.so.6")
  set_target_properties(qt_abi_probe PROPERTIES BUILD_RPATH "${IDALINGO_QT_DIR}")
  add_test(NAME qt_abi_probe COMMAND qt_abi_probe)
  add_test(NAME qt_preload_translation
    COMMAND ${CMAKE_COMMAND} -E env
      "IDALINGO_LANGUAGE=${IDALINGO_LANGUAGE_FILE}"
      "LD_PRELOAD=$<TARGET_FILE:idalingo>"
      $<TARGET_FILE:qt_abi_probe> --expect-translated)
  add_test(NAME launcher_preload_translation
    COMMAND ${CMAKE_COMMAND} -E env
      "IDALINGO_IDA_EXECUTABLE=$<TARGET_FILE:qt_abi_probe>"
      $<TARGET_FILE:idalingo_launcher> --expect-translated)
endif()

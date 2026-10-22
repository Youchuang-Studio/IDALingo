add_library(idalingo_translation STATIC
  common/translation.cpp
)
target_compile_features(idalingo_translation PUBLIC cxx_std_20)
target_include_directories(idalingo_translation PUBLIC "${PROJECT_SOURCE_DIR}/include")
set_target_properties(idalingo_translation PROPERTIES POSITION_INDEPENDENT_CODE ON)
idalingo_enable_warnings(idalingo_translation)

add_library(idalingo_text_exporter STATIC
  common/text_exporter.cpp
)
target_compile_features(idalingo_text_exporter PUBLIC cxx_std_20)
target_include_directories(idalingo_text_exporter PUBLIC "${PROJECT_SOURCE_DIR}/include")
set_target_properties(idalingo_text_exporter PROPERTIES POSITION_INDEPENDENT_CODE ON)
idalingo_enable_warnings(idalingo_text_exporter)

if(WIN32)
  add_subdirectory(windows)
else()
  add_subdirectory(linux)
endif()

install(FILES "${IDALINGO_LANGUAGE_FILE}" DESTINATION . RENAME language.txt)

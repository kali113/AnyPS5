add_library(libSceOpusCeltDec SHARED EXCLUDE_FROM_ALL
    Export.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSceOpusDec/src/Decoder.cpp
)
target_include_directories(libSceOpusCeltDec PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(libSceOpusCeltDec PRIVATE Opus::opus)
if(NOT APPLE)
    target_link_options(libSceOpusCeltDec PRIVATE LINKER:--exclude-libs,ALL)
endif()
configure_windows_unwind(libSceOpusCeltDec)
set_target_properties(libSceOpusCeltDec PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
set_source_files_properties(Export.cpp PROPERTIES COMPILE_OPTIONS -fvisibility=default)

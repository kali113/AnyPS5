add_library(libSceOpusDec SHARED EXCLUDE_FROM_ALL
    Export.cpp
    src/Decoder.cpp
)
target_include_directories(libSceOpusDec PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(libSceOpusDec PRIVATE Opus::opus)
if(NOT APPLE)
    target_link_options(libSceOpusDec PRIVATE LINKER:--exclude-libs,ALL)
endif()
configure_windows_unwind(libSceOpusDec)
set_target_properties(libSceOpusDec PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)

if(BUILD_TESTING)
    add_test_executable(guest_opus_dec_tests ${LIBS_INCLUDE_DIR}/tests/GuestOpusDec.cpp)
    target_include_directories(guest_opus_dec_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_opus_dec_tests PRIVATE libSceOpusDec libSceOpusCeltDec libc)
    configure_windows_unwind(guest_opus_dec_tests)
    if(MINGW)
        target_compile_options(guest_opus_dec_tests PRIVATE -fasynchronous-unwind-tables)
    endif()
    add_test(NAME guest_opus_dec COMMAND guest_opus_dec_tests)
    if(WIN32)
        set_tests_properties(guest_opus_dec PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceOpusDec>")
    endif()
endif()
set_source_files_properties(Export.cpp PROPERTIES COMPILE_OPTIONS -fvisibility=default)

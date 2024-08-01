function(add_qtduckdb_properties TARGET)
    find_package(Qt${QT_VERSION} REQUIRED COMPONENTS Test Sql Widgets)
    target_link_libraries(${TARGET} PRIVATE Qt::Sql QtDuckDBDriver)
    set_property(TARGET ${TARGET} PROPERTY AUTOMOC ON)
    set_property(TARGET ${TARGET} PROPERTY CXX_STANDARD 14)

    if (WIN32)
        add_custom_command(TARGET ${TARGET} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:${TARGET}> $<TARGET_FILE_DIR:${TARGET}>
                COMMAND_EXPAND_LISTS)
    endif()

    add_custom_command(TARGET ${TARGET} POST_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory 
                "$<TARGET_FILE_DIR:${TARGET}>/plugins/sqldrivers/")
    add_custom_command(TARGET ${TARGET} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:QtDuckDBDriver> "$<TARGET_FILE_DIR:${TARGET}>/plugins/sqldrivers/")

    if (NOT WIN32) # seems like duckdb disabled sanitizer on windows build
        if (ENABLE_SANITIZER)
            target_compile_options(${TARGET} PRIVATE -fsanitize=address)
            target_link_options(${TARGET} PRIVATE -fsanitize=address)
        endif()

        if (ENABLE_UBSAN)
            target_compile_options(${TARGET} PRIVATE -fsanitize=undefined)
            target_link_options(${TARGET} PRIVATE -fsanitize=undefined)
        endif()
endif(NOT WIN32)


endfunction()
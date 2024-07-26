find_package(Qt${QT_VERSION} REQUIRED COMPONENTS Test Sql Widgets Concurrent)

add_executable(Example_AutoComplete "main.cpp")

target_link_libraries(Example_AutoComplete PRIVATE Qt::Test Qt::Sql Qt::Widgets Qt::Concurrent QtDuckDBDriver)
set_property(TARGET Example_AutoComplete PROPERTY AUTOMOC ON)

if (WIN32)
    add_custom_command(TARGET Example_AutoComplete POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:Example_AutoComplete> $<TARGET_FILE_DIR:Example_AutoComplete>
            COMMAND_EXPAND_LISTS)
endif()

add_custom_command(TARGET Example_AutoComplete POST_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory 
            "$<TARGET_FILE_DIR:Example_AutoComplete>/plugins/sqldrivers/")
add_custom_command(TARGET Example_AutoComplete POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:QtDuckDBDriver> "$<TARGET_FILE_DIR:Example_AutoComplete>/plugins/sqldrivers/")


if (ENABLE_SANITIZER)
    target_compile_options(Example_AutoComplete PRIVATE -fsanitize=address)
    target_link_options(Example_AutoComplete PRIVATE -fsanitize=address)
endif()

if (ENABLE_UBSAN)
    target_compile_options(Example_AutoComplete PRIVATE -fsanitize=undefined)
    target_link_options(Example_AutoComplete PRIVATE -fsanitize=undefined)
endif()
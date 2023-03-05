find_package(Qt6 REQUIRED COMPONENTS Test Sql Widgets Concurrent)
qt_standard_project_setup()

add_executable(Example_TableWidget "main.cpp")

target_link_libraries(Example_TableWidget PRIVATE Qt::Test Qt::Sql Qt::Widgets Qt::Concurrent QtDuckDBDriver)
set_property(TARGET Example_TableWidget PROPERTY AUTOMOC ON)

if (WIN32)
    add_custom_command(TARGET Example_TableWidget POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:Example_TableWidget> $<TARGET_FILE_DIR:Example_TableWidget>
            COMMAND_EXPAND_LISTS)
endif()

add_custom_command(TARGET Example_TableWidget POST_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory 
            "$<TARGET_FILE_DIR:Example_TableWidget>/plugins/sqldrivers/")
add_custom_command(TARGET Example_TableWidget POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:QtDuckDBDriver> "$<TARGET_FILE_DIR:Example_TableWidget>/plugins/sqldrivers/")


if (ENABLE_SANITIZER)
    target_compile_options(Example_TableWidget PRIVATE -fsanitize=address)
    target_link_options(Example_TableWidget PRIVATE -fsanitize=address)
endif()

if (ENABLE_UBSAN)
    target_compile_options(Example_TableWidget PRIVATE -fsanitize=undefined)
    target_link_options(Example_TableWidget PRIVATE -fsanitize=undefined)
endif()
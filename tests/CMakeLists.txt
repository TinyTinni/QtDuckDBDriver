
find_package(Qt6 REQUIRED COMPONENTS Test Sql)
qt_standard_project_setup()

add_executable(driver_tests "driver_tests.h" "main.cpp")
add_test(NAME driver_tests COMMAND driver_tests)

target_link_libraries(driver_tests PRIVATE Qt::Test Qt::Sql QtDuckDBDriver)
set_property(TARGET driver_tests PROPERTY AUTOMOC ON)

if (WIN32)
    add_custom_command(TARGET driver_tests POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:driver_tests> $<TARGET_FILE_DIR:driver_tests>
            COMMAND_EXPAND_LISTS)
endif()

add_custom_command(TARGET driver_tests POST_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory 
            "$<TARGET_FILE_DIR:driver_tests>/plugins/sqldrivers/")
add_custom_command(TARGET driver_tests POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:QtDuckDBDriver> "$<TARGET_FILE_DIR:driver_tests>/plugins/sqldrivers/")

if (ENABLE_SANITIZER)
    target_compile_options(driver_tests PRIVATE -fsanitize=address)
    target_link_options(driver_tests PRIVATE -fsanitize=address)
endif()

if (ENABLE_UBSAN)
    target_compile_options(driver_tests PRIVATE -fsanitize=undefined)
    target_link_options(driver_tests PRIVATE -fsanitize=undefined)
endif()


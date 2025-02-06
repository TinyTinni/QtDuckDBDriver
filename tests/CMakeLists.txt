add_executable(driver_tests "driver_tests.h" "main.cpp")
add_test(NAME driver_tests COMMAND driver_tests)
add_qtduckdb_properties(driver_tests)
target_link_libraries(driver_tests PRIVATE Qt::Test)


# for duckdb internal access
if (WIN32)
    target_link_libraries(driver_tests PRIVATE QtDuckDBDriver)
else()
    target_link_libraries(driver_tests PRIVATE duckdb_static)
    target_include_directories(driver_tests PRIVATE $<TARGET_PROPERTY:QtDuckDBDriver,INCLUDE_DIRECTORIES>)
endif()
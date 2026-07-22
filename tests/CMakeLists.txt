include(../cmake/get_cpm.cmake)

# Add RapidCheck for property-based testing
CPMAddPackage(
    NAME rapidcheck
    GITHUB_REPOSITORY emil-e/rapidcheck
    GIT_TAG b2d9ed2dddefc4b84318d664b4f221eb792d89c7
    SYSTEM YES
    OPTIONS "RC_ENABLE_TESTS OFF" "RC_ENABLE_GTEST_TESTS OFF"
)

# =============================================================================
# Helper function: add_rc_test(NAME SOURCE)
#   Adds a RapidCheck test executable with standard linkage and environment.
# =============================================================================
function(add_rc_test NAME SOURCE)
    add_executable(${NAME} ${SOURCE})
    add_test(NAME ${NAME} COMMAND ${NAME})
    set_tests_properties(${NAME} PROPERTIES ENVIRONMENT "${RC_TEST_ENV}")
    add_qtduckdb_properties(${NAME})
    target_link_libraries(${NAME} PRIVATE Qt::Test rapidcheck)

    if (WIN32)
        target_link_libraries(${NAME} PRIVATE QtDuckDBDriver)
    else()
        target_link_libraries(${NAME} PRIVATE duckdb_static ${QTDUCKDB_EXTENSION_LINK_TARGETS})
        target_include_directories(${NAME} SYSTEM PRIVATE $<TARGET_PROPERTY:QtDuckDBDriver,INCLUDE_DIRECTORIES>)
    endif()
endfunction()

# =============================================================================
# Qt Test integration tests (single executable)
# =============================================================================
add_executable(driver_tests
    main_qttest.cpp
    qttest/query_execution_test.cpp
    qttest/prepared_statements_test.cpp
    qttest/schema_test.cpp
    qttest/model_test.cpp
    qttest/raw_handle_test.cpp
    qttest/features_test.cpp
    qttest/error_handling_test.cpp
)

add_test(NAME driver_tests COMMAND driver_tests)
set_tests_properties(driver_tests PROPERTIES TIMEOUT 600)
add_qtduckdb_properties(driver_tests)
target_link_libraries(driver_tests PRIVATE Qt::Test)

# for duckdb internal access (needed by raw_handle_test)
if (WIN32)
    target_link_libraries(driver_tests PRIVATE QtDuckDBDriver)
else()
    target_link_libraries(driver_tests PRIVATE duckdb_static ${QTDUCKDB_EXTENSION_LINK_TARGETS})
    target_include_directories(driver_tests SYSTEM PRIVATE $<TARGET_PROPERTY:QtDuckDBDriver,INCLUDE_DIRECTORIES>)
endif()

# =============================================================================
# RapidCheck property-based tests
# =============================================================================

# DuckDB intentionally leaks internal state; suppress LeakSanitizer for RC tests
# so CTest doesn't report false failures from DuckDB-internal allocations.
set(RC_TEST_ENV "LSAN_OPTIONS=detect_leaks=0:allocator_may_return_null=1")

add_rc_test(rc_type_roundtrip          rapidcheck/type_roundtrip_test.cpp)
add_rc_test(rc_identifier_escaping     rapidcheck/identifier_escaping_test.cpp)
add_rc_test(rc_prepared_statements     rapidcheck/prepared_statements_test.cpp)
add_rc_test(rc_null_handling           rapidcheck/null_handling_test.cpp)

# =============================================================================
# RapidCheck stateful property-based tests
# =============================================================================

add_rc_test(rc_stateful_lifecycle      rapidcheck/stateful_lifecycle_test.cpp)
add_rc_test(rc_stateful_transaction    rapidcheck/stateful_transaction_test.cpp)
add_rc_test(rc_stateful_prepared       rapidcheck/stateful_prepared_test.cpp)

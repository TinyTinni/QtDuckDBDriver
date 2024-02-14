# CMakeList.txt : CMake project for QtDuckDBDriver, include source and define
# project specific logic here.
#

find_package(Qt6 REQUIRED COMPONENTS Sql)
set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};$ENV{QTDIR}")

add_subdirectory(duckdb)

add_library (QtDuckDBDriver SHARED "QtDuckDBDriver.cpp"  "smain.cpp")
target_sources(QtDuckDBDriver PUBLIC FILE_SET include_those TYPE HEADERS FILES "QtDuckDBDriver.h")

set_property(TARGET QtDuckDBDriver PROPERTY CXX_STANDARD 14)
target_link_libraries(QtDuckDBDriver PUBLIC Qt6::Sql duckdb_static)
target_link_libraries(QtDuckDBDriver PRIVATE sqlite3_api_wrapper_static ${DUCKDB_EXTRA_LINK_FLAGS})
target_include_directories(QtDuckDBDriver PRIVATE "${Qt6Sql_PRIVATE_INCLUDE_DIRS}")
target_include_directories(QtDuckDBDriver PRIVATE "duckdb/tools/sqlite3_api_wrapper/include" "duckdb/tools/sqlite3_api_wrapper/sqlite3_udf_api/include")
target_compile_definitions(QtDuckDBDriver PRIVATE QT_PLUGIN)
set_property(TARGET QtDuckDBDriver PROPERTY AUTOMOC ON)
set_property(TARGET QtDuckDBDriver PROPERTY DEBUG_POSTFIX "d")
set_property(TARGET QtDuckDBDriver PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
set_property(TARGET QtDuckDBDriver PROPERTY INTERPROCEDURAL_OPTIMIZATION_DEBUG FALSE)


if (NOT WIN32) # seems like duckdb disabled sanitizer on windows build
    if (ENABLE_SANITIZER)
        target_compile_options(QtDuckDBDriver PRIVATE -fsanitize=address)
        target_link_options(QtDuckDBDriver PRIVATE -fsanitize=address)
    endif()

    if (ENABLE_UBSAN)
        target_compile_options(QtDuckDBDriver PRIVATE -fsanitize=undefined)
        target_link_options(QtDuckDBDriver PRIVATE -fsanitize=undefined)
    endif()
endif(NOT WIN32)

install(TARGETS QtDuckDBDriver RUNTIME DESTINATION "bin")
install(FILES "QtDuckDBDriver.h" DESTINATION "include")
install(FILES ../README.md ../LICENSE DESTINATION ".")
install(DIRECTORY "${PROJECT_SOURCE_DIR}/QtDuckDBDriver/duckdb/src/include/"
          DESTINATION "include")
include(CPack)

ADD_CUSTOM_TARGET( bundle
         COMMAND "${CMAKE_CPACK_COMMAND}"
                 "-C" "$<CONFIGURATION>" "-G" "7Z"
            "--config" "${CMAKE_BINARY_DIR}/CPackConfig.cmake"
            DEPENDS ${PROJECT_NAME} QtDuckDBDriver)
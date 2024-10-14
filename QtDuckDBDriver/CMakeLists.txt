# CMakeList.txt : CMake project for QtDuckDBDriver, include source and define
# project specific logic here.
#

find_package(Qt${QT_VERSION} REQUIRED COMPONENTS Sql)

include(../cmake/get_cpm.cmake)
CPMAddPackage("gh:duckdb/duckdb#v1.1.2")

add_library (QtDuckDBDriver SHARED "QtDuckDBDriver.cpp"  "smain.cpp")
target_sources(QtDuckDBDriver PUBLIC FILE_SET include_those TYPE HEADERS FILES "QtDuckDBDriver.h")

set_property(TARGET QtDuckDBDriver PROPERTY CXX_STANDARD 14)
target_link_libraries(QtDuckDBDriver PUBLIC Qt::Sql duckdb_static)
target_include_directories(QtDuckDBDriver PRIVATE "${Qt${QT_VERSION}Sql_PRIVATE_INCLUDE_DIRS}")
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
install(DIRECTORY "${duckdb_SOURCE_DIR}/src/include/"
          DESTINATION "include")
include(CPack)

ADD_CUSTOM_TARGET( bundle
         COMMAND "${CMAKE_CPACK_COMMAND}"
                 "-C" "$<CONFIGURATION>" "-G" "7Z"
            "--config" "${CMAKE_BINARY_DIR}/CPackConfig.cmake"
            DEPENDS ${PROJECT_NAME} QtDuckDBDriver)

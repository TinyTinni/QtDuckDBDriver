# CMakeList.txt : CMake project for QtDuckDBDriver, include source and define
# project specific logic here.
#

find_package(Qt${QT_VERSION} REQUIRED COMPONENTS Sql)

add_library(QtSqlPrivate INTERFACE)
target_include_directories(QtSqlPrivate INTERFACE "${Qt${QT_VERSION}Sql_PRIVATE_INCLUDE_DIRS}")
set_property(TARGET QtSqlPrivate PROPERTY INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${Qt${QT_VERSION}Sql_PRIVATE_INCLUDE_DIRS}")

include(../cmake/get_cpm.cmake)
CPMAddPackage("gh:duckdb/duckdb#v${QTDUCKDB_DUCKDB_VERSION}")

get_property(duckdb_include_dirs TARGET duckdb_static PROPERTY INTERFACE_INCLUDE_DIRECTORIES)
set_property(TARGET duckdb_static PROPERTY INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${duckdb_include_dirs}")

add_library (QtDuckDBDriver SHARED "QtDuckDBDriver.cpp"  "smain.cpp")
target_sources(QtDuckDBDriver PUBLIC FILE_SET include_those TYPE HEADERS FILES "QtDuckDBDriver.h")

set_property(TARGET QtDuckDBDriver PROPERTY CXX_STANDARD 17)
target_link_libraries(QtDuckDBDriver PUBLIC Qt::Sql duckdb_static QtSqlPrivate)
target_compile_definitions(QtDuckDBDriver PRIVATE QT_PLUGIN)
set_property(TARGET QtDuckDBDriver PROPERTY AUTOMOC ON)
set_property(TARGET QtDuckDBDriver PROPERTY DEBUG_POSTFIX "d")
set_property(TARGET QtDuckDBDriver PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
set_property(TARGET QtDuckDBDriver PROPERTY INTERPROCEDURAL_OPTIMIZATION_DEBUG FALSE)
set_property(TARGET QtDuckDBDriver PROPERTY COMPILE_WARNING_AS_ERROR ${QTDUCKDB_WARNING_AS_ERRORS})

# disables sign conversion for qt5 as qt6 is the future and they fixed a lot of data types there
if (NOT "${QT_VERSION}" STREQUAL "5")
    target_compile_options(QtDuckDBDriver PRIVATE
            $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
                -Wall -Wextra -Wconversion -pedantic-errors -Wsign-conversion>
            $<$<CXX_COMPILER_ID:MSVC>:/W4>
        )
endif()


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

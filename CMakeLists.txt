# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.23)

set(CMAKE_PROJECT_INCLUDE_BEFORE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EnableProjectCMP0048.cmake")
project ("QtDuckDBDriver" VERSION 0.2.1)

# Match the static CRT linkage used by DuckDB (/MT) across all targets,
# including tests, to avoid LNK2038 RuntimeLibrary mismatches on MSVC.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(QTDUCKDB_BUILD_EXAMPLES OFF)
option(QTDUCKDB_BUILD_TESTS ON)
option(QTDUCKDB_WARNING_AS_ERRORS OFF)
set(QTDUCKDB_DUCKDB_VERSION "1.5.5" CACHE STRING "Version of DuckDB which should be included")

include(cmake/DetectQtVersion.cmake)
include(cmake/QtDuckDBProperties.cmake)

# Include sub-projects.
add_subdirectory ("QtDuckDBDriver")
if(QTDUCKDB_BUILD_TESTS)
    include(CTest)
    add_subdirectory("tests")
endif()

if (QTDUCKDB_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

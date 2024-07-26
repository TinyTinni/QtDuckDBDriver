# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.23)

set(CMAKE_PROJECT_INCLUDE_BEFORE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EnableProjectCMP0048.cmake")
project ("QtDuckDBDriver" VERSION 0.2.1) 

option(QTDUCKDB_BUILD_EXAMPLES OFF)
option(QTDUCKDB_BUILD_TESTS ON)
include(cmake/DetectQtVersion.cmake)

# Include sub-projects.
add_subdirectory ("QtDuckDBDriver")
if(QTDUCKDB_BUILD_TESTS)
    include(CTest)
    add_subdirectory("tests")
endif()

if (QTDUCKDB_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.21)

project ("QtDuckDBDriver")

option(BUILD_EXAMPLES OFF)

# Include sub-projects.
add_subdirectory ("QtDuckDBDriver")
include(CTest)
if(BUILD_TESTING)
    add_subdirectory("tests")
endif()

if (BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
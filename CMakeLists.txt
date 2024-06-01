# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.23)

set(CMAKE_PROJECT_INCLUDE_BEFORE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EnableProjectCMP0048.cmake")
project ("QtDuckDBDriver" VERSION 0.2.0) 

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

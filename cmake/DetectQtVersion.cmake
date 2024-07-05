# detect qt version
find_package(Qt6 QUIET)
set(_QT_VERSION 6)
if(NOT Qt6_FOUND)
    message("Qt6 not found, trying Qt5")
    find_package(Qt5 QUIET)
    if (Qt5_FOUND)
        set(_QT_VERSION 5)
        message("Qt5 found, using Qt5")
    endif()
endif()

set(QT_VERSION ${_QT_VERSION} CACHE STRING "Qt Version")
message(STATUS "Using Qt${QT_VERSION}")
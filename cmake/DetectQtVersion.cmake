# detect qt version
set(_QT_VERSION 6)

find_package(Qt6 QUIET COMPONENTS Core)
if(NOT Qt6_FOUND)
    message(STATUS "Qt6 not found, trying Qt5")
    find_package(Qt5 QUIET COMPONENTS Core)
    if (Qt5_FOUND)
        set(_QT_VERSION 5)
    else()
        message(WARNING "Cannot find any Qt Version")
    endif()
endif()

set(QT_VERSION ${_QT_VERSION} CACHE STRING "Qt Version")
message(STATUS "Using Qt${QT_VERSION}")
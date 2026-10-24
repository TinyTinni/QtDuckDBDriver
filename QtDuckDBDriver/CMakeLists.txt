
if (${QTDUCKDB_QT_VERSION} STREQUAL "5")

    find_package(Qt${QTDUCKDB_QT_VERSION} REQUIRED COMPONENTS Sql)
    
    add_library(QtSqlPrivate INTERFACE)
    target_include_directories(QtSqlPrivate INTERFACE "${Qt${QTDUCKDB_QT_VERSION}Sql_PRIVATE_INCLUDE_DIRS}")
    set_property(TARGET QtSqlPrivate PROPERTY INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${Qt${QTDUCKDB_QT_VERSION}Sql_PRIVATE_INCLUDE_DIRS}")
    set(QTPrivateSql QtSqlPrivate)

else()

    find_package(Qt${QTDUCKDB_QT_VERSION} REQUIRED COMPONENTS Sql SqlPrivate)
    set(QTPrivateSql Qt::SqlPrivate)

endif()

set(QTDUCKDB_DUCKDB_EXTENSIONS "autocomplete" CACHE STRING "Additional DuckDB extensions built into the bundled DuckDB")
set(_qtduckdb_build_extensions ${QTDUCKDB_DUCKDB_EXTENSIONS})
if (DEFINED BUILD_EXTENSIONS AND NOT "${BUILD_EXTENSIONS}" STREQUAL "")
    list(APPEND _qtduckdb_build_extensions ${BUILD_EXTENSIONS})
endif()
list(REMOVE_DUPLICATES _qtduckdb_build_extensions)
set(BUILD_EXTENSIONS "${_qtduckdb_build_extensions}" CACHE STRING "DuckDB extensions built into the bundled DuckDB" FORCE)

include(../cmake/get_cpm.cmake)
CPMAddPackage("gh:duckdb/duckdb#v${QTDUCKDB_DUCKDB_VERSION}")
include(GNUInstallDirs)

set(QTDUCKDB_PLUGIN_INSTALL_DIR "sqldrivers" CACHE STRING "Install directory for the Qt SQL driver plugin")
set(QTDUCKDB_PACKAGE_SUFFIX "${CMAKE_SYSTEM_NAME}-Qt${QTDUCKDB_QT_VERSION}" CACHE STRING "Suffix used for package filenames")
set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}-${QTDUCKDB_PACKAGE_SUFFIX}")

# DuckDB 2.0 links the extensions into a target with duckdb_link_extensions(): it adds the
# extension archives, generates the loader that registers them and adds the autoloader that
# registers them before main. The helper needs the DuckDB source directory in the scope it is
# called from, DuckDB only sets it in its own directory scope.
set(DUCKDB_MODULE_BASE_DIR "${duckdb_SOURCE_DIR}")
set(DUCKDB_MODULE_BASE_DIR "${duckdb_SOURCE_DIR}" PARENT_SCOPE)

set(QTDUCKDB_EXTENSION_NAMES)
foreach(_qtduckdb_extension IN LISTS QTDUCKDB_DUCKDB_EXTENSIONS)
    list(APPEND QTDUCKDB_EXTENSION_NAMES ${_qtduckdb_extension})
endforeach()
foreach(_qtduckdb_builtin_extension IN ITEMS core_functions parquet)
    list(APPEND QTDUCKDB_EXTENSION_NAMES ${_qtduckdb_builtin_extension})
endforeach()
list(REMOVE_DUPLICATES QTDUCKDB_EXTENSION_NAMES)
set(QTDUCKDB_EXTENSION_NAMES "${QTDUCKDB_EXTENSION_NAMES}" CACHE INTERNAL "DuckDB extensions linked into the Qt SQL driver" FORCE)

# DuckDB's bundled fmt library instantiates std::char_traits with a non-standard
# type, triggering -Wdeprecated-declarations on macOS (Apple libc++). Suppress
# this only on DuckDB's own extension targets where it originates.
if(APPLE)
    foreach(_ext_name IN LISTS QTDUCKDB_EXTENSION_NAMES)
        if(TARGET ${_ext_name}_extension)
            target_compile_options(${_ext_name}_extension PRIVATE -Wno-deprecated-declarations)
        endif()
    endforeach()
endif()

add_library (QtDuckDBDriver SHARED "QtDuckDBDriver.cpp"  "smain.cpp")
target_sources(QtDuckDBDriver PUBLIC FILE_SET include_those TYPE HEADERS FILES "QtDuckDBDriver.h")

#duckdb_static will not link the header file (neither .h nor .hpp). We have to add them manually
target_include_directories(QtDuckDBDriver SYSTEM PUBLIC "${duckdb_SOURCE_DIR}/src/include")

set_property(TARGET QtDuckDBDriver PROPERTY CXX_STANDARD 17)
target_link_libraries(QtDuckDBDriver PUBLIC Qt::Sql duckdb_static)
duckdb_link_extensions(QtDuckDBDriver PRIVATE ${QTDUCKDB_EXTENSION_NAMES})
target_link_libraries(QtDuckDBDriver PRIVATE ${QTPrivateSql})
target_compile_definitions(QtDuckDBDriver PRIVATE QT_PLUGIN)
# DuckDB 2.0 declares a member named "slots", which Qt's "slots" macro would expand to
# nothing. Building without the Qt keywords keeps the DuckDB headers intact.
target_compile_definitions(QtDuckDBDriver PRIVATE QT_NO_KEYWORDS)
target_compile_definitions(QtDuckDBDriver PUBLIC DUCKDB_STATIC_BUILD)
set_property(TARGET QtDuckDBDriver PROPERTY AUTOMOC ON)
set_property(TARGET QtDuckDBDriver PROPERTY DEBUG_POSTFIX "d")
set_property(TARGET QtDuckDBDriver PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
set_property(TARGET QtDuckDBDriver PROPERTY INTERPROCEDURAL_OPTIMIZATION_DEBUG FALSE)
set_property(TARGET QtDuckDBDriver PROPERTY COMPILE_WARNING_AS_ERROR ${QTDUCKDB_WARNING_AS_ERRORS})

# disables sign conversion for qt5 as qt6 is the future and they fixed a lot of data types there
if (NOT "${QTDUCKDB_QT_VERSION}" STREQUAL "5")
    target_compile_options(QtDuckDBDriver PRIVATE
            $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
                -Wall -Wextra -Wconversion -pedantic-errors -Wsign-conversion>
            $<$<CXX_COMPILER_ID:MSVC>:/W4>
        )
endif()

target_compile_options(QtDuckDBDriver PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
    )


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

install(TARGETS QtDuckDBDriver
        RUNTIME DESTINATION "${QTDUCKDB_PLUGIN_INSTALL_DIR}"
        LIBRARY DESTINATION "${QTDUCKDB_PLUGIN_INSTALL_DIR}"
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
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

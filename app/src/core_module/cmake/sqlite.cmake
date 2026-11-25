if(ORION_USE_SYSTEM_DEPS)
    return()
endif()

ExternalProject_Add(sqlite_external
	SOURCE_DIR ${PROJECT_SOURCE_DIR}/third_party/sqlite
    # These options are crucial for flatpak build (because cmake version is older in flatpak)
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
)
ExternalProject_Get_property(sqlite_external SOURCE_DIR)
ExternalProject_Get_property(sqlite_external BINARY_DIR)

if(NOT EXISTS ${SOURCE_DIR}/sqlite/configure)
message("Missing: ${SOURCE_DIR}/sqlite/configure")
message(FATAL_ERROR "Missing sqlite submodule, to fix run:\n$ git submodule update --init app/third_party/sqlite/sqlite")
endif()

message("/configure sqlite")
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    # Build out-of-source to keep the sqlite submodule clean
    set(_sqlite_build_dir "${BINARY_DIR}/sqlite_build")
    if(NOT EXISTS ${_sqlite_build_dir}/sqlite3.c)
        find_program(VSWHERE_EXE vswhere
            PATHS "$ENV{ProgramFiles\(x86\)}/Microsoft Visual Studio/Installer"
        )
        if(NOT VSWHERE_EXE)
            message(FATAL_ERROR "vswhere not found. Visual Studio must be installed.")
        endif()
        execute_process(
            COMMAND "${VSWHERE_EXE}" -latest -property installationPath
            OUTPUT_VARIABLE _vs_install_path
            OUTPUT_STRIP_TRAILING_WHITESPACE
            COMMAND_ERROR_IS_FATAL ANY
        )

        # Copy sqlite source to build directory
        file(COPY ${SOURCE_DIR}/sqlite/ DESTINATION ${_sqlite_build_dir})

        cmake_path(NATIVE_PATH _sqlite_build_dir _build_native)
        set(_vcvarsall "${_vs_install_path}/VC/Auxiliary/Build/vcvarsall.bat")
        cmake_path(NATIVE_PATH _vcvarsall _vcvarsall_native)
        set(_cmd "${BINARY_DIR}/build_sqlite_x64.cmd")
        cmake_path(NATIVE_PATH _cmd _cmd_native)

        set(_sqlite_dir "${_build_native}")
        set(_makefile   "${_sqlite_dir}\\Makefile.msc")

        configure_file(cmake/build_sqlite_x64.cmd.in ${BINARY_DIR}/build_sqlite_x64.cmd)
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E env ${_cmd_native}
            WORKING_DIRECTORY "${_sqlite_build_dir}"
            COMMAND_ECHO STDOUT
            COMMAND_ERROR_IS_FATAL ANY
        )
    endif()
    set(SQLITE3_DIR ${_sqlite_build_dir})
else()
    execute_process(
        COMMAND ${SOURCE_DIR}/sqlite/configure --prefix=${CMAKE_INSTALL_PREFIX} --includedir=${CMAKE_INSTALL_PREFIX}/include/sqlite --enable-static --disable-tcl
        WORKING_DIRECTORY ${BINARY_DIR}
        COMMAND_ECHO STDOUT
        COMMAND_ERROR_IS_FATAL ANY
    )
    if(NOT EXISTS ${BINARY_DIR}/sqlite3.c)
        message("make")
        execute_process(
            COMMAND make sqlite3.c
            WORKING_DIRECTORY ${BINARY_DIR}
            COMMAND_ECHO STDOUT
            COMMAND_ERROR_IS_FATAL ANY
        )
    endif()
    set(SQLITE3_DIR ${BINARY_DIR})
endif()

configure_file(${SQLITE3_DIR}/sqlite3.c ${PROJECT_SOURCE_DIR}/third_party/cmake_sqlite/sqlite3.c COPYONLY)
configure_file(${SQLITE3_DIR}/sqlite3.h ${PROJECT_SOURCE_DIR}/third_party/cmake_sqlite/sqlite3.h COPYONLY)
configure_file(${SQLITE3_DIR}/sqlite3ext.h ${PROJECT_SOURCE_DIR}/third_party/cmake_sqlite/sqlite3ext.h COPYONLY)
configure_file(cmake/sqlite.CMakeLists.txt.in ${PROJECT_SOURCE_DIR}/third_party/cmake_sqlite/CMakeLists.txt COPYONLY)
add_submodule(cmake_sqlite third_party/cmake_sqlite)


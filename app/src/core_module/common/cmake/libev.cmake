ExternalProject_Add(libev_external
	SOURCE_DIR ${PROJECT_SOURCE_DIR}/third_party/libev
    # These options are crucial for flatpak build (because cmake version is older in flatpak)
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
)
ExternalProject_Get_property(libev_external SOURCE_DIR)
ExternalProject_Get_property(libev_external BINARY_DIR)

if(NOT EXISTS ${SOURCE_DIR}/libev/configure)
message("Missing: ${SOURCE_DIR}/libev/configure")
message(FATAL_ERROR "Missing libev submodule, to fix run:\n$ git submodule update --init app/third_party/libev/libev")
endif()

message("libev: /configure")
execute_process(
    # TODO: this does not work - still cannot link libev static library
    COMMAND /bin/sh -c "set CFLAGS=-fPIC && ${SOURCE_DIR}/libev/configure --prefix=${CMAKE_INSTALL_PREFIX} --includedir=${CMAKE_INSTALL_PREFIX}/include/libev"
    #ENV CFLAGS="-fPIC"
    WORKING_DIRECTORY ${BINARY_DIR}
    COMMAND_ECHO STDOUT
    COMMAND_ERROR_IS_FATAL ANY
)

message("libev: make")
execute_process(
    COMMAND make
    WORKING_DIRECTORY ${BINARY_DIR}
    COMMAND_ECHO STDOUT
    COMMAND_ERROR_IS_FATAL ANY
)
    
if(NOT EXISTS ${CMAKE_INSTALL_PREFIX}/lib/libev.a)
    message("libev: make install")
    execute_process(
        COMMAND make install
        WORKING_DIRECTORY ${BINARY_DIR}
        COMMAND_ECHO STDOUT
        COMMAND_ERROR_IS_FATAL ANY
    )
endif()

add_library(libev IMPORTED STATIC)

set_target_properties(libev PROPERTIES
    IMPORTED_LOCATION ${CMAKE_INSTALL_PREFIX}/lib/libev.a
)

target_include_directories(libev INTERFACE
    ${CMAKE_INSTALL_PREFIX}/include
)

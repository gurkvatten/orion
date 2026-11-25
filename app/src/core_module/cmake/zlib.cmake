ExternalProject_Add(zlib_external
	SOURCE_DIR ${PROJECT_SOURCE_DIR}/third_party/zlib
    # These options are crucial for flatpak build (because cmake version is older in flatpak)
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
)
ExternalProject_Get_property(zlib_external SOURCE_DIR)
ExternalProject_Get_property(zlib_external BINARY_DIR)

if(NOT EXISTS ${SOURCE_DIR}/CMakeLists.txt)
    message("Missing: ${SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "Missing zlib submodule, to fix run:\n$ git submodule update --init third_party/zlib")
endif()

unset(CMAKE_CONFIGURE_ARGS)
if(CMAKE_GENERATOR)
    list(APPEND CMAKE_CONFIGURE_ARGS -G "${CMAKE_GENERATOR}")
endif()
if(CMAKE_GENERATOR_TOOLSET)
    list(APPEND CMAKE_CONFIGURE_ARGS -T "${CMAKE_GENERATOR_TOOLSET}")
endif()
if(CMAKE_GENERATOR_PLATFORM)
    list(APPEND CMAKE_CONFIGURE_ARGS -A "${CMAKE_GENERATOR_PLATFORM}")
endif()
list(APPEND CMAKE_CONFIGURE_ARGS -S "${SOURCE_DIR}")
list(APPEND CMAKE_CONFIGURE_ARGS -B "${BINARY_DIR}")
if(CMAKE_BUILD_TYPE)
    list(APPEND CMAKE_CONFIGURE_ARGS -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE})
endif()
if(CMAKE_INSTALL_PREFIX)
    list(APPEND CMAKE_CONFIGURE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX})
endif()

message("zlib: cmake")
execute_process(
    COMMAND ${CMAKE_COMMAND} ${CMAKE_CONFIGURE_ARGS} -DZLIB_BUILD_TESTING=0 -DZLIB_BUILD_SHARED=0
    WORKING_DIRECTORY ${BINARY_DIR}
    COMMAND_ECHO STDOUT
    COMMAND_ERROR_IS_FATAL ANY
)

message("zlib: cmake --build .")
execute_process(
    COMMAND ${CMAKE_COMMAND} --build . --config Release
    WORKING_DIRECTORY ${BINARY_DIR}
    COMMAND_ECHO STDOUT
    COMMAND_ERROR_IS_FATAL ANY
)

message("zlib: install")
execute_process(
    COMMAND ${CMAKE_COMMAND} --install . --config Release
    WORKING_DIRECTORY ${BINARY_DIR}
    COMMAND_ECHO STDOUT
    COMMAND_ERROR_IS_FATAL ANY
)

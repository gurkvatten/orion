if(EXISTS ${LIBBACKTRACE_SOURCE_DIR}/configure)
ExternalProject_Add(LibBacktrace
	SOURCE_DIR ${LIBBACKTRACE_SOURCE_DIR}
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
)
ExternalProject_Get_property(LibBacktrace SOURCE_DIR)
ExternalProject_Get_property(LibBacktrace BINARY_DIR)
if(NOT EXISTS ${BINARY_DIR}/Makefile)
    execute_process(
        COMMAND ${SOURCE_DIR}/configure --prefix=${CMAKE_INSTALL_PREFIX}
        WORKING_DIRECTORY ${BINARY_DIR}
        COMMAND_ECHO STDOUT
        COMMAND_ERROR_IS_FATAL ANY
    )
endif()
if(NOT EXISTS ${BINARY_DIR}/.libs/libbacktrace.a)
    execute_process(
        COMMAND make -j
        WORKING_DIRECTORY ${BINARY_DIR}
        COMMAND_ECHO STDOUT
        COMMAND_ERROR_IS_FATAL ANY
    )
endif()
if(NOT EXISTS ${CMAKE_INSTALL_PREFIX}/lib/libbacktrace.a)
    execute_process(
        COMMAND make install
        WORKING_DIRECTORY ${BINARY_DIR}
        COMMAND_ECHO STDOUT
        COMMAND_ERROR_IS_FATAL ANY
    )
endif()
endif()

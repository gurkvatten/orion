if(EXISTS ${UNIFDEF_SOURCE_DIR}/Makefile)
ExternalProject_Add(Unifdef
	SOURCE_DIR ${UNIFDEF_SOURCE_DIR}
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
)
ExternalProject_Get_property(Unifdef SOURCE_DIR)
ExternalProject_Get_property(Unifdef BINARY_DIR)
configure_file(${UNIFDEF_SOURCE_DIR}/../../cmake/gtk/unifdef.version.sh ${UNIFDEF_SOURCE_DIR}/version.sh COPYONLY)
if(NOT EXISTS ${SOURCE_DIR}/unifdef)
    set(CFLAGS "CFLAGS+=-std=gnu17")
    execute_process(
        COMMAND make ${CFLAGS}
        WORKING_DIRECTORY ${SOURCE_DIR}
        COMMAND_ECHO STDOUT
        COMMAND_ERROR_IS_FATAL ANY
    )
endif()
if(NOT EXISTS ${CMAKE_INSTALL_PREFIX}/bin/unifdef)
    execute_process(
        COMMAND make install DESTDIR=${CMAKE_INSTALL_PREFIX} prefix=
        WORKING_DIRECTORY ${SOURCE_DIR}
        COMMAND_ECHO STDOUT
        COMMAND_ERROR_IS_FATAL ANY
    )
endif()
endif()
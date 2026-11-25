file(WRITE ${PROJECT_SOURCE_DIR}/.vscode/flatpak.application.id.txt ${APPLICATION_ID})
file(WRITE ${PROJECT_SOURCE_DIR}/.vscode/flatpak.debug.port.txt ${FLATPAK_DEBUG_PORT})

set(FLATPAK_INSTALL_DIR ${CMAKE_BINARY_DIR}/install)
set(FLATPAK_BINARY_DIR ${CMAKE_BINARY_DIR}/build)
set(FLATPAK_REPO_DIR ${CMAKE_BINARY_DIR}/repo)
set(FLATPAK_EXPORT_DIR ${CMAKE_BINARY_DIR}/export)

if(NOT EXISTS ${FLATPAK_BINARY_DIR} OR NOT EXISTS ${FLATPAK_INSTALL_DIR})
    file(REMOVE_RECURSE ${FLATPAK_BINARY_DIR} ${FLATPAK_INSTALL_DIR})
    file(MAKE_DIRECTORY ${FLATPAK_BINARY_DIR})
    
    execute_process(
        COMMAND flatpak remote-add --if-not-exists --user flathub https://dl.flathub.org/repo/flathub.flatpakrepo
        COMMAND_ECHO STDOUT
        WORKING_DIRECTORY ${FLATPAK_BINARY_DIR}
        COMMAND_ERROR_IS_FATAL ANY
    )
    # SDK package reference and version should not be hard-coded here, use CMake variables instead.
    execute_process(
        COMMAND flatpak info --user ${FLATPAK_SDK}//${FLATPAK_RUNTIME_VERSION}
        RESULT_VARIABLE FLATPAK_SDK_RC
        COMMAND_ECHO STDOUT
        WORKING_DIRECTORY ${FLATPAK_BINARY_DIR}
        OUTPUT_QUIET
        ERROR_QUIET
    )
    message(STATUS ${FLATPAK_SDK_RC})
    if(NOT FLATPAK_SDK_RC EQUAL 0)
        execute_process(
            COMMAND flatpak install --user -y flathub
                ${FLATPAK_RUNTIME}//${FLATPAK_RUNTIME_VERSION}
                ${FLATPAK_SDK}//${FLATPAK_RUNTIME_VERSION}
            COMMAND_ECHO STDOUT
            WORKING_DIRECTORY ${FLATPAK_BINARY_DIR}
            COMMAND_ERROR_IS_FATAL ANY
        )
    endif()
    execute_process(
        COMMAND flatpak build-init ${FLATPAK_INSTALL_DIR} ${APPLICATION_ID} ${FLATPAK_SDK} ${FLATPAK_RUNTIME} ${FLATPAK_RUNTIME_VERSION}
        COMMAND_ECHO STDOUT
        WORKING_DIRECTORY ${FLATPAK_BINARY_DIR}
        COMMAND_ERROR_IS_FATAL ANY
    )
    execute_process(
        COMMAND flatpak build --share=network ${FLATPAK_INSTALL_DIR} cmake -G Ninja
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DCMAKE_INSTALL_PREFIX=/app
            -DENABLE_WEBKIT=${ENABLE_WEBKIT}
            # When adding/changing/removing CMake variables here, always delete build/<preset> directory to ensure
            # that the changes take effect. Otherwise, CMake cache will not be updated and the changes will be ignored.
            -DORION_COMMON_USE_GLIB=${ORION_COMMON_USE_GLIB}
            -DFLATPAK_BUILD=1
            ${CMAKE_SOURCE_DIR}
        COMMAND_ECHO STDOUT
        WORKING_DIRECTORY ${FLATPAK_BINARY_DIR}
        COMMAND_ERROR_IS_FATAL ANY
    )
endif()

set(APPICATION_SCREENSHOTS_GLUE_OPEN "<screenshot><image>")
set(APPICATION_SCREENSHOTS_GLUE_OPEN_DEFAULT "<screenshot type=\"default\"><image>")
set(APPICATION_SCREENSHOTS_GLUE_CLOSE "</image></screenshot>")
list(JOIN APPICATION_SCREENSHOTS ${APPICATION_SCREENSHOTS_GLUE_CLOSE}${APPICATION_SCREENSHOTS_GLUE_OPEN} APPICATION_SCREENSHOTS_GLUED)
set(APPICATION_SCREENSHOTS_RESULT ${APPICATION_SCREENSHOTS_GLUE_OPEN_DEFAULT}${APPICATION_SCREENSHOTS_GLUED}${APPICATION_SCREENSHOTS_GLUE_CLOSE})

configure_file(cmake/app.desktop.in ${FLATPAK_INSTALL_DIR}/files/share/applications/${APPLICATION_ID}.desktop)
configure_file(cmake/app.metainfo.xml.in ${FLATPAK_INSTALL_DIR}/files/share/metainfo/${APPLICATION_ID}.metainfo.xml)
configure_file(cmake/app_icon_16.png ${FLATPAK_INSTALL_DIR}/files/share/icons/hicolor/16x16/apps/${APPLICATION_ID}.png COPYONLY)
configure_file(cmake/app_icon_32.png ${FLATPAK_INSTALL_DIR}/files/share/icons/hicolor/32x32/apps/${APPLICATION_ID}.png COPYONLY)
configure_file(cmake/app_icon_64.png ${FLATPAK_INSTALL_DIR}/files/share/icons/hicolor/64x64/apps/${APPLICATION_ID}.png COPYONLY)
configure_file(cmake/app_icon_128.png ${FLATPAK_INSTALL_DIR}/files/share/icons/hicolor/128x128/apps/${APPLICATION_ID}.png COPYONLY)
configure_file(cmake/app_icon_256.png ${FLATPAK_INSTALL_DIR}/files/share/icons/hicolor/256x256/apps/${APPLICATION_ID}.png COPYONLY)

add_custom_target(flatpak-build ALL
    COMMAND flatpak build --share=network ${FLATPAK_INSTALL_DIR} cmake --build . --target install
    WORKING_DIRECTORY ${FLATPAK_BINARY_DIR}
    USES_TERMINAL
    VERBATIM
)

# --no-partial-urls --media-baseurl=https://dl.flathub.org/media/ --media-dir=${FLATPAK_INSTALL_DIR}/files/share/app-info/media
add_custom_target(flatpak-export ALL
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${FLATPAK_EXPORT_DIR}
    COMMAND cp -a ${FLATPAK_INSTALL_DIR} ${FLATPAK_EXPORT_DIR}
    COMMAND flatpak build ${FLATPAK_EXPORT_DIR} rm -rf /app/share/doc /app/share/man /app/include
    COMMAND flatpak build ${FLATPAK_EXPORT_DIR} find /app -type f -name "*.a" -delete
    COMMAND flatpak build ${FLATPAK_EXPORT_DIR} sh -c "find /app -type f \\( -name '*.so' -o -name '*.so.*' -o -name '*.elf' \\) -print0 | xargs -0 -r strip --strip-unneeded"
    COMMAND appstreamcli compose --prefix=/ --origin=${APPLICATION_ID} --result-root=${FLATPAK_EXPORT_DIR}/files --data-dir=${FLATPAK_EXPORT_DIR}/files/share/app-info/xmls --icons-dir=${FLATPAK_EXPORT_DIR}/files/share/app-info/icons/flatpak --components=${APPLICATION_ID} ${FLATPAK_EXPORT_DIR}/files

    # TODO: uncomment this when all errors are fixed
    # Uses --pedantic flag to actually fail even on pedantic-severity issues:
    #COMMAND appstreamcli validate --pedantic ${FLATPAK_EXPORT_DIR}/files/share/metainfo/${APPLICATION_ID}.metainfo.xml

    COMMAND flatpak build-finish ${FLATPAK_EXPORT_DIR}
        --command=${APPLICATION_COMMAND}
        --share=ipc
        --share=network
        --socket=fallback-x11
        --socket=wayland
        --socket=pulseaudio
        #--socket=system-bus  # webcam access attempt
        #--socket=session-bus # webcam access attempt
        --talk-name=org.freedesktop.secrets
        --talk-name=org.freedesktop.portal.Secret
        --talk-name=org.freedesktop.portal.Camera  # webcam access attempt
        --talk-name=org.freedesktop.portal.Desktop # webcam access attempt
        #--talk-name=org.freedesktop.portal.ScreenCast
        #When commenting out permissions its requiered to delete build dir for changes to take effect
        #--socket=cups
        #--socket=system-bus
        #--talk-name=org.gtk.vfs.*
        #--filesystem=xdg-run/gvfsd
        # Epiphany sets this to --filesystem=xdg-download (--filesystem=host is too permissive).
        # ~/.var/app/${APPLICATION_ID}/ directory should remain accessible.
        --filesystem=xdg-download
        # Epiphany sets this to --device=dri
        --device=all
        #--env=XDG_CURRENT_DESKTOP=GNOME # webcam access attempt
        #--env=GDK_BACKEND=wayland       # webcam access attempt
        # Epiphany sets this to --own-name=org.gnome.Epiphany.WebAppProvider for communication with its service
        #--own-name=${APPLICATION_ID}
    COMMAND flatpak build-export ${FLATPAK_REPO_DIR} ${FLATPAK_EXPORT_DIR}
    COMMAND flatpak build-bundle ${FLATPAK_REPO_DIR} ${CMAKE_CURRENT_BINARY_DIR}/${APPLICATION_COMMAND}.flatpak ${APPLICATION_ID}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_INSTALL_PREFIX}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/${APPLICATION_COMMAND}.flatpak ${CMAKE_INSTALL_PREFIX}/${APPLICATION_COMMAND}.flatpak
    WORKING_DIRECTORY ${FLATPAK_BINARY_DIR}
    DEPENDS flatpak-build
    USES_TERMINAL
    VERBATIM
)

add_executable(${APPLICATION_COMMAND}
    cmake/dummy.c
)

add_dependencies(${APPLICATION_COMMAND} flatpak-export)

set_target_properties(${APPLICATION_COMMAND} PROPERTIES
	OUTPUT_NAME "__dummy__"
)

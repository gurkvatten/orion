include(GNUInstallDirs)
add_library(WebKit STATIC ${WEBKIT_SOURCE_DIR}/../../cmake/dummy.cpp)
install(TARGETS WebKit EXPORT orion_coreConfig
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
if(ENABLE_WEBKIT)
	set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${WEBKIT_SOURCE_DIR}/Source/cmake")
#   the find_package wouldn't find libsoup aparently not using pkg-config to do so
#   hence calling directly the pkg-config supplied method which obviously succeeds
#	find_package(LibSoup 3.0.0 REQUIRED)
    pkg_check_modules(LIBSOUP REQUIRED libsoup-3.0)
    if (NOT(LIBSOUP_FOUND EQUAL 1))
        message(FATAL_ERROR "libsoup-3.0.pc not found")
    endif()

    set(WEBKIT_SOURCE_DIR "${WEBKIT_SOURCE_DIR}")
    set(WEBKIT_BINARY_DIR "${CMAKE_BINARY_DIR}/WebKit")

	ExternalProject_Add(
		WebKit_ExternalProject
		BUILD_ALWAYS 1
		SOURCE_DIR "${WEBKIT_SOURCE_DIR}"
        BINARY_DIR "${WEBKIT_BINARY_DIR}"
		CONFIGURE_COMMAND cmake
            -DPORT=GTK
            -DCMAKE_BUILD_TYPE=Release
			-DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}
			-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
            -DCMAKE_INSTALL_RPATH=${CMAKE_INSTALL_RPATH}
			-DENABLE_SPEECH_SYNTHESIS=0
            -DENABLE_WK_WEB_EXTENSIONS=1
            -G Ninja
            -S "${WEBKIT_SOURCE_DIR}"
            -B "${WEBKIT_BINARY_DIR}"
		BUILD_COMMAND cmake --build "${WEBKIT_BINARY_DIR}"
		INSTALL_COMMAND cmake --build "${WEBKIT_BINARY_DIR}" --target install
		USES_TERMINAL_PATCH 1
		USES_TERMINAL_CONFIGURE 1
		USES_TERMINAL_BUILD 1
		LOG_INSTALL 1
	)
	set(WebKit_FOUND 1)
	set_target_properties(WebKit PROPERTIES OUTPUT_NAME "_dummy_")
	target_include_directories(WebKit PUBLIC
		${CMAKE_INSTALL_FULL_INCLUDEDIR}/webkitgtk-6.0
		${LIBSOUP_INCLUDE_DIRS}
	)
	target_link_directories(WebKit
        PUBLIC ${CMAKE_INSTALL_FULL_LIBDIR})
	target_link_libraries(WebKit
		"libwebkitgtk-6.0.so"
		"libjavascriptcoregtk-6.0.so"
	)
	target_compile_definitions(WebKit PUBLIC
		ENABLE_WEBKIT=1
	)
	add_dependencies(WebKit WebKit_ExternalProject)
endif()

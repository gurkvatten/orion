cmake_minimum_required (VERSION 3.24)

project(superbuild)
include(Manifest.cmake)

if(FLATPAK_HOST)
    include(cmake/flatpak.cmake)
else()
    add_subdirectory(app)
endif()


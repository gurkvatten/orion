# Windows dependencies
#add_compile_definitions(CURL_STATICLIB)

# temporary workaround to export all symbols
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

find_package(CURL REQUIRED)

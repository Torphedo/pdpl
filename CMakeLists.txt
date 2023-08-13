cmake_minimum_required(VERSION 3.16)
project(phantom_dust_plugin_loader
	VERSION 0.2.0
	LANGUAGES C
)

set(CMAKE_C_STANDARD 99)
set(CMAKE_SHARED_LIBRARY_PREFIX "") # Remove "lib" prefix from output filename

add_subdirectory(ext/minhook)
add_subdirectory(pdpl)

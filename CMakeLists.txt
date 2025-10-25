cmake_minimum_required(VERSION 3.16)
project(pdpl
	VERSION 0.3.0
	LANGUAGES C
)

set(CMAKE_C_STANDARD 99) # Our code is C99

add_executable(pdpl
    src/main.c
    src/dll.c
    src/process.c
)

add_subdirectory(ext/bobtail)
target_link_libraries(pdpl PRIVATE bobtail)
cmake_minimum_required(VERSION 3.16)
project(pdpl
	VERSION 0.2.0
	LANGUAGES C
)

set(CMAKE_C_STANDARD 99) # Our code is C99
set(CMAKE_SHARED_LIBRARY_PREFIX "") # Remove "lib" prefix from output filename

# Compile minhook.
add_subdirectory(ext/minhook)

add_executable(pdpl
  "src/main.c"
  "src/self_inject.c"
  "src/hooks.c"
  "src/path.c"
)
target_include_directories(pdpl PRIVATE "ext/minhook")
target_link_libraries(pdpl PRIVATE "-s" minhook)


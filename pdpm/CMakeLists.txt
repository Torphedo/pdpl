add_library(pdpm SHARED
	"src/dll_main.c"
	"src/console.c"
	"src/hooks.c"
	"src/path.c"
	"src/filesystem.c"
	"src/atomic_log.c"
)

target_link_libraries(pdpm PRIVATE minhook physfs-static)
target_include_directories(pdpm PRIVATE "../ext/minhook/include" "../ext/physfs/src")

# This auto-generates the "pdpm.sha512" and "pdpm.md5" files in "inject/hash/" so that they're updated each build
add_custom_command(TARGET pdpm POST_BUILD COMMAND ${CMAKE_COMMAND} -P ${CMAKE_SOURCE_DIR}/inject/hash/hash.cmake)

add_custom_command(TARGET pdpm POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/pdpm.dll ${CMAKE_CURRENT_BINARY_DIR}/../inject/pdpm.dll)

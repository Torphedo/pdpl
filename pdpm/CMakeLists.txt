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

add_custom_command(TARGET pdpm POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/pdpm.dll ${CMAKE_CURRENT_BINARY_DIR}/../inject/pdpm.dll)

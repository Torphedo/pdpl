add_library(pdpm SHARED
	"src/dll_main.c"
	"src/console.c"
	"src/hooks.c"
	"src/path.c"
	"src/filesystem.c"
	"src/plugins.c"
)

target_link_libraries(pdpm PRIVATE "-s" minhook physfs-static MemoryModule)
target_include_directories(pdpm PRIVATE "../ext/minhook/include" "../ext/physfs/src" "../ext/MemoryModule")

# This object file needs to be rebuilt to force it to include the new pdpm.dll on each build.
add_custom_command(
		TARGET pdpm
		POST_BUILD
		COMMAND del "..\\inject\\CMakeFiles\\inject.dir\\src\\remote_injection.c.obj"
)

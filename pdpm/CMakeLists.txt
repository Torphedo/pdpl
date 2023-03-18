add_library(pdpm SHARED
	"src/dll_main.c"
	"src/console.c"
	"src/hooks.c"
	"src/path.c"
	"src/filesystem.c"
	"src/atomic_log.c"
	"src/injection.c"
)

target_link_libraries(pdpm PRIVATE "-s" minhook physfs-static MemoryModule)
target_include_directories(pdpm PRIVATE "../ext/minhook/include" "../ext/physfs/src" "../ext/MemoryModule")

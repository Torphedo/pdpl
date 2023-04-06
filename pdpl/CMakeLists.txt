add_executable(pdpl
	"src/inject/main.c"
	"src/inject/self_inject.c"

	"src/loader_init.c"
	"src/console.c"
	"src/hooks.c"
	"src/path.c"
	"src/filesystem.c"
	"src/plugins.c"
)

target_link_libraries(pdpl PRIVATE "-s" minhook physfs-static MemoryModule)
target_include_directories(pdpl PRIVATE "../ext/minhook/include" "../ext/physfs/src" "../ext/MemoryModule")

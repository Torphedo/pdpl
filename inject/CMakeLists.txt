add_executable(inject
	"src/inject/main.c"
	"src/inject/self_inject.c"

	"src/loader_init.c"
	"src/console.c"
	"src/hooks.c"
	"src/path.c"
	"src/filesystem.c"
	"src/plugins.c"
)

target_link_libraries(inject PRIVATE "-s" minhook physfs-static MemoryModule)
target_include_directories(inject PRIVATE "../ext/minhook/include" "../ext/physfs/src" "../ext/MemoryModule")

target_compile_definitions(inject PRIVATE PDPM_DLL="${CMAKE_CURRENT_BINARY_DIR}/../pdpl/pdpl.dll")

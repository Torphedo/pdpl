add_library(pdpm SHARED
	"src/dll_main.c"
	"src/console.c"
    "src/hooks.c"
)

target_link_libraries(pdpm PRIVATE minhook physfs-static)
target_include_directories(pdpm PRIVATE "../ext/minhook/include" "../ext/physfs/src")

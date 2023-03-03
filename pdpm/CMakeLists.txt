add_library(pdpm SHARED
	"src/dll_main.c"
	"src/console.c"
)

target_link_libraries(pdpm PRIVATE minhook)
target_include_directories(pdpm PRIVATE "../ext/minhook/include")

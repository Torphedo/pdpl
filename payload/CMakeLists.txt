add_library(payload SHARED
	"src/dll_main.c"
)

target_link_libraries(payload PRIVATE minhook)
target_include_directories(payload PRIVATE "../ext/minhook/include")

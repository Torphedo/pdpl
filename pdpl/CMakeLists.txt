add_executable(pdpl
	"src/main.c"
	"src/self_inject.c"
	"src/hooks.c"
	"src/path.c"
)
target_include_directories(pdpl PRIVATE "../ext/minhook")
target_link_libraries(pdpl PRIVATE "-s" minhook)

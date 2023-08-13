add_executable(inject
	"src/main.c"
	"src/self_inject.c"
	"src/hooks.c"
	"src/path.c"
)
target_include_directories(inject PRIVATE "../ext/incbin" "../ext/minhook")
target_link_libraries(inject PRIVATE "-s" minhook)

target_compile_definitions(inject PRIVATE PDPM_DLL="${CMAKE_CURRENT_BINARY_DIR}/../pdpl/pdpl.dll")

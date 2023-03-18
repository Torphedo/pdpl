add_executable(inject
	"src/main.c"
	"src/remote_injection.c"
)
target_include_directories(inject PRIVATE "../ext/incbin")

target_compile_definitions(inject PRIVATE PDPM_DLL="${CMAKE_CURRENT_BINARY_DIR}/../pdpm/pdpm.dll")

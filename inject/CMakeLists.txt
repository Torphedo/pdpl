add_executable(inject
	"src/main.c"
	"src/remote_injection.c"
)

add_custom_command(TARGET inject POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/../pdpm/pdpm.dll ${CMAKE_CURRENT_BINARY_DIR}/mods/plugins/pdpm.dll)

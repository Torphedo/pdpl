add_executable(inject
	"src/main.c"
	"src/remote_injection.c"
	"hash/hashes.c"

	"../ext/sphlib/c/sha2big.c"
	"../ext/sphlib/c/md5.c"
)

# This happens at CMake generation time right now. Ideally, this would be updated on each build instead,
# via a post-build command or something similar (like in pdpm's script that copies build output here)
file(SHA512 ${CMAKE_CURRENT_BINARY_DIR}/pdpm.dll PDPM_SHA512)
file(MD5 ${CMAKE_CURRENT_BINARY_DIR}/pdpm.dll PDPM_MD5)

target_compile_definitions(inject PUBLIC PDPM_SHA512="${PDPM_SHA512}")
target_compile_definitions(inject PUBLIC PDPM_MD5="${PDPM_MD5}")
target_include_directories(inject PRIVATE "../ext/sphlib/c")

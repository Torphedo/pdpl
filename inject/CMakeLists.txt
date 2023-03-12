add_executable(inject
	"src/main.c"
	"src/remote_injection.c"
	"hash/hashes.c"

	"../ext/sphlib/c/sha2big.c"
	"../ext/sphlib/c/md5.c"
)

target_include_directories(inject PRIVATE "../ext/sphlib/c")

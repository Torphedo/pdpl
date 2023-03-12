cmake_minimum_required(VERSION 3.16)
project(phantom_dust_plugin_manager
	VERSION 0.0.1
	LANGUAGES C
)

set(CMAKE_C_STANDARD 99)
set(CMAKE_SHARED_LIBRARY_PREFIX "") # Remove "lib" prefix from output filename

# Build statically without docs, to reduce build time.
set(PHYSFS_BUILD_DOCS FALSE)
set(PHYSFS_BUILD_STATIC TRUE)
set(PHYSFS_BUILD_SHARED FALSE)

# Disable everything but zip and 7z support. This saves about 12KiB in the final DLL build.
set(PHYSFS_ARCHIVE_GRP FALSE)
set(PHYSFS_ARCHIVE_WAD FALSE)
set(PHYSFS_ARCHIVE_MVL FALSE)
set(PHYSFS_ARCHIVE_HOG FALSE)
set(PHYSFS_ARCHIVE_SLB FALSE)
set(PHYSFS_ARCHIVE_VDF FALSE)
set(PHYSFS_ARCHIVE_QPAK FALSE)
set(PHYSFS_ARCHIVE_ISO9660 FALSE)
add_subdirectory(ext/physfs)
add_subdirectory(ext/minhook)
add_subdirectory(ext/MemoryModule)

add_subdirectory(pdpm)
add_subdirectory(inject)
add_subdirectory(test_plugin)


file(SHA512 ${CMAKE_BINARY_DIR}/pdpm.dll PDPM_SHA512)
file(MD5 ${CMAKE_BINARY_DIR}/pdpm.dll PDPM_MD5)

file(WRITE ${CMAKE_BINARY_DIR}/../../../inject/hash/pdpm.sha512 \"${PDPM_SHA512}\")
file(WRITE ${CMAKE_BINARY_DIR}/../../../inject/hash/pdpm.md5 \"${PDPM_MD5}\")

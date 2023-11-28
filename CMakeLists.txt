cmake_minimum_required (VERSION 3.1.0)

project (k1x-vpu-test)

# Add subdirectory.
add_subdirectory(test/utils)
add_subdirectory(test/md5)
add_subdirectory(test/userptr)
add_subdirectory(test/mvx_player)
add_subdirectory(tools/logd)

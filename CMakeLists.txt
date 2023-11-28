cmake_minimum_required (VERSION 3.1.0)

project (mvx-test)

#set(CMAKE_SYSTEM_NAME Linux)
#set(CMAKE_SYSTEM_PROCESSOR riscv64)

#set(CROSS_COMPILE riscv64-unknown-linux-gnu-)
#set(CMAKE_CROSSCOMPILING True)
#set(CMAKE_C_COMPILER ${CROSS_COMPILE}gcc)
#set(CMAKE_CXX_COMPILER ${CROSS_COMPILE}g++)
#set(CMAKE_STRIP ${CROSS_COMPILE}strip)
#set(CMAKE_AR ${CROSS_COMPILE}ar)
#set(CMAKE_RANLIB ${CROSS_COMPILE}ranlib)

# Add subdirectory.
add_subdirectory(test/utils)
add_subdirectory(test/md5)
add_subdirectory(test/userptr)
add_subdirectory(test/mvx_player)
add_subdirectory(tools/logd)

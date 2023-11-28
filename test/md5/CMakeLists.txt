# CMakeLists.txt

# Set library sources.
set(LIB_SOURCES "md5.c")

# Build static library.
add_library(mvxmd5 STATIC "${LIB_SOURCES}")

# Set include directories.
target_include_directories(mvxmd5 PUBLIC "${CMAKE_CURRENT_LIST_DIR}")


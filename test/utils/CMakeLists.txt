# CMakeLists.txt

# Set library sources.
set(LIB_SOURCES "mvx_argparse.c")

# Build static library.
add_library(mvxutils STATIC "${LIB_SOURCES}")

# Set include directories.
target_include_directories(mvxutils PUBLIC "${CMAKE_CURRENT_LIST_DIR}")


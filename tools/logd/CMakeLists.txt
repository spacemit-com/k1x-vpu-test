# CMakeLists.txt

# Set sources.
set(SOURCES "mvx_logd.cpp" "mvx_logd_fwif_v2.cpp")

# Build executable.
add_executable(mvx_logd ${SOURCES})

# Add include directories.
target_include_directories(mvx_logd PRIVATE "${CMAKE_SOURCE_DIR}/include")

# Install target.
install(TARGETS mvx_logd RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")

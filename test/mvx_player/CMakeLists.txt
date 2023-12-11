# CMakeLists.txt

include_directories(
    ${CMAKE_SOURCE_DIR}/include/
)

# Set library sources.
set(LIB_SOURCES "mvx_player.cpp" "dmabufheap/BufferAllocator.cpp" "dmabufheap/BufferAllocatorWrapper.cpp")

# Build object library.
add_library(mvx_player_obj OBJECT "${LIB_SOURCES}")

# Build executables.
add_executable(mvx_decoder "mvx_decoder.cpp")
target_link_libraries(mvx_decoder PRIVATE mvx_player_obj mvxutils mvxmd5)

add_executable(mvx_decoder_multi "mvx_decoder_multi.cpp")
target_link_libraries(mvx_decoder_multi PRIVATE mvx_player_obj mvxutils mvxmd5)

add_executable(mvx_encoder "mvx_encoder.cpp")
target_link_libraries(mvx_encoder PRIVATE mvx_player_obj mvxutils mvxmd5)

add_executable(mvx_encoder_gen "mvx_encoder_gen.cpp")
target_link_libraries(mvx_encoder_gen PRIVATE mvx_player_obj mvxutils mvxmd5)

add_executable(mvx_encoder_multi "mvx_encoder_multi.cpp")
target_link_libraries(mvx_encoder_multi PRIVATE mvx_player_obj mvxutils mvxmd5)

add_executable(mvx_info "mvx_info.cpp")
target_link_libraries(mvx_info PRIVATE mvx_player_obj mvxutils mvxmd5)

install(TARGETS mvx_decoder
		mvx_decoder_multi
		mvx_encoder
		mvx_encoder_multi
		RUNTIME
		DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")

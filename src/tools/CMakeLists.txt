add_executable(align_image_stack align_image_stack.cpp)
target_link_libraries(align_image_stack ${common_libs} ${image_libs} ${OPENGL_GLEW_LIBRARIES})

install(TARGETS align_image_stack DESTINATION ${BINDIR})

add_executable(shader_compiler)
target_link_libraries(shader_compiler PRIVATE penumbra_gpu slang::slang)
target_sources(shader_compiler PRIVATE main.cpp)
set_target_properties(shader_compiler PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

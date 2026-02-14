add_executable(editor)

target_link_libraries(editor penumbra::modules)
target_sources(editor PRIVATE main.cpp)
set_target_properties(editor PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

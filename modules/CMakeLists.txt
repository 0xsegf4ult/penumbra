add_library(penumbra INTERFACE "")

add_subdirectory("core")
add_subdirectory("math")
add_subdirectory("gpu")
add_subdirectory("ui")
add_subdirectory("renderer")

add_library(penumbra::modules ALIAS penumbra)

target_compile_features(penumbra INTERFACE cxx_std_23)
target_link_libraries(penumbra INTERFACE penumbra_core penumbra_math penumbra_gpu penumbra_ui penumbra_renderer)

add_library(penumbra_physics STATIC "")

target_link_libraries(penumbra_physics PRIVATE penumbra_core PUBLIC box3d::box3d)
target_include_directories(penumbra_physics PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_sources(penumbra_physics PRIVATE impl_box3d.cpp)

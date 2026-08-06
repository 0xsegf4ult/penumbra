add_library(penumbra_physics STATIC "")

target_link_libraries(penumbra_physics PRIVATE penumbra_core penumbra_math)

target_sources(penumbra_physics PUBLIC FILE_SET CXX_MODULES
	FILES
	broadphase/broadphase.cppm
	broadphase/bvh4_node.cppm
	broadphase/bvh4_tree.cppm
	broadphase/bvh4_tree_build.cppm
	collision/shape.cppm
	collision/shape_create.cppm
	collision/gjk.cppm
	collision/sat.cppm
	api.cppm
	body.cppm
	world.cppm
	mod.cppm)

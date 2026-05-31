add_library(penumbra_anim STATIC "")

target_link_libraries(penumbra_anim PRIVATE penumbra_core penumbra_math)

target_sources(penumbra_anim PUBLIC FILE_SET CXX_MODULES
	FILES
	animation.cppm
	skeleton.cppm
	mod.cppm)

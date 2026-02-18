add_library(penumbra_math STATIC "")

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	set(PENUMBRA_MATH_LIBRARIES ${PENUMBRA_MATH_LIBRARIES} m)
endif()

target_link_libraries(penumbra_math PUBLIC ${PENUMBRA_MATH_LIBRARIES})
target_sources(penumbra_math PUBLIC FILE_SET CXX_MODULES FILES
	matrix.cppm
	quaternion.cppm
	transform.cppm
	vector.cppm
	utility.cppm
	mod.cppm)


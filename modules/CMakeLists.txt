add_library(penumbra INTERFACE "")

add_custom_command(
        OUTPUT
		${CMAKE_SOURCE_DIR}/include/penumbra/config.hpp
                ALL
        COMMAND
	${CMAKE_COMMAND} -D IN_FILE=${CMAKE_SOURCE_DIR}/include/penumbra/config.hpp.in -D OUT_FILE=${CMAKE_SOURCE_DIR}/include/penumbra/config.hpp -P ${PROJECT_SOURCE_DIR}/cmake/GenerateConfig.cmake
        WORKING_DIRECTORY
                ${PROJECT_SOURCE_DIR}
)

add_custom_target(
	generate_config_module ALL DEPENDS ${CMAKE_SOURCE_DIR}/include/penumbra/config.hpp
)

add_dependencies(penumbra generate_config_module)

add_subdirectory("core")
add_subdirectory("gpu")
add_subdirectory("ui")
add_subdirectory("renderer")
add_subdirectory("ecs")
add_subdirectory("physics")
add_subdirectory("resource")

add_library(penumbra::modules ALIAS penumbra)

target_compile_features(penumbra INTERFACE cxx_std_23)
target_include_directories(penumbra INTERFACE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(penumbra INTERFACE penumbra_core penumbra_gpu penumbra_ui penumbra_renderer penumbra_ecs penumbra_physics penumbra_resource)

add_library(penumbra STATIC "")

add_custom_command(
        OUTPUT
		${PROJECT_SOURCE_DIR}/include/penumbra/config.hpp
                ALL
        COMMAND
	${CMAKE_COMMAND} -D IN_FILE=${PROJECT_SOURCE_DIR}/include/penumbra/config.hpp.in -D OUT_FILE=${PROJECT_SOURCE_DIR}/include/penumbra/config.hpp -P ${PROJECT_SOURCE_DIR}/cmake/GenerateConfig.cmake
        WORKING_DIRECTORY
                ${PROJECT_SOURCE_DIR}
)

add_custom_target(
	generate_config_module ALL DEPENDS ${PROJECT_SOURCE_DIR}/include/penumbra/config.hpp
)

add_dependencies(penumbra generate_config_module)

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	set(PENUMBRA_CORE_LIBRARIES ${PENUMBRA_CORE_LIBRARIES} pthread m)
endif()

find_package(SDL3 REQUIRED)

add_library(penumbra::engine ALIAS penumbra)

target_compile_features(penumbra PUBLIC cxx_std_23)
target_include_directories(penumbra PUBLIC ${PROJECT_SOURCE_DIR}/include PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(penumbra PUBLIC Tracy::TracyClient spdlog::spdlog ${PENUMBRA_CORE_LIBRARIES} EnTT::EnTT box3d::box3d imgui PRIVATE SDL3::SDL3 volk)
target_sources(penumbra PRIVATE
	audio/device.cpp
	core/cmd.cpp
	core/cvar.cpp
	core/init.cpp
	core/input.cpp
	core/input_translate.cpp
	core/memory.cpp
	core/panic.cpp
	core/vfs.cpp
	core/window.cpp
	gpu/shader.cpp
	gpu/vulkan.cpp
	physics/impl_box3d.cpp
	renderer/bloom.cpp
	renderer/brdf.cpp
	renderer/resource.cpp
	renderer/renderer.cpp
	renderer/shadowmap.cpp
	renderer/transparent.cpp
	renderer/visbuffer.cpp
	renderer/world.cpp
	resource/resource.cpp
	ui/device_overlay.cpp
	ui/imgui.cpp)

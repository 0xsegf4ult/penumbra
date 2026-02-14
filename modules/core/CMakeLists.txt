add_library(penumbra_core STATIC "")

add_custom_command(
        OUTPUT
                ${CMAKE_CURRENT_LIST_DIR}/config.cppm
                ALL
        COMMAND
                ${CMAKE_COMMAND} -D IN_FILE=${CMAKE_CURRENT_LIST_DIR}/config.cppm.in -D OUT_FILE=${CMAKE_CURRENT_LIST_DIR}/config.cppm -P ${PROJECT_SOURCE_DIR}/cmake/GenerateConfig.cmake
        WORKING_DIRECTORY
                ${PROJECT_SOURCE_DIR}
)

add_custom_target(
        generate_config_module ALL DEPENDS ${CMAKE_CURRENT_LIST_DIR}/config.cppm
)

add_dependencies(penumbra_core generate_config_module)

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	set(PENUMBRA_CORE_LIBRARIES ${PENUMBRA_CORE_LIBRARIES} pthread)
endif()

find_package(SDL3 REQUIRED)

target_link_libraries(penumbra_core PUBLIC ${PENUMBRA_CORE_LIBRARIES} PRIVATE spdlog::spdlog penumbra_math SDL3::SDL3)
target_sources(penumbra_core PUBLIC FILE_SET CXX_MODULES FILES
	array_proxy.cppm
	config.cppm
	hash.cppm
	log.cppm
	strongly_typed.cppm
	vfs.cppm
	window.cppm
	mod.cppm)

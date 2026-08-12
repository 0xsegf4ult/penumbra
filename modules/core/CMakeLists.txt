add_library(penumbra_core STATIC "")

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	set(PENUMBRA_CORE_LIBRARIES ${PENUMBRA_CORE_LIBRARIES} pthread m)
endif()

find_package(SDL3 REQUIRED)

target_link_libraries(penumbra_core PUBLIC Tracy::TracyClient ${PENUMBRA_CORE_LIBRARIES} spdlog::spdlog PRIVATE SDL3::SDL3)
target_include_directories(penumbra_core PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_sources(penumbra_core
	PRIVATE
	cvar.cpp
	input.cpp
	panic.cpp
	vfs.cpp
	window.cpp)

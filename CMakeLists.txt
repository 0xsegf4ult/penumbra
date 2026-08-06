cmake_minimum_required(VERSION 3.30.5 FATAL_ERROR)

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
set(CMAKE_CXX_STANDARD 23)

project(penumbra VERSION 0.1 LANGUAGES CXX)

include(FetchContent)

option(PENUMBRA_BUILD_TOOLS "Build tools" ON)
option(PENUMBRA_BUILD_EDITOR "Build editor" ON)
option(PENUMBRA_ENABLE_TRACY "Enable Tracy" OFF)

add_subdirectory("external")
add_subdirectory("modules")

if(PENUMBRA_BUILD_TOOLS)
	add_subdirectory("tools")
endif()

if(PENUMBRA_BUILD_EDITOR)
	add_subdirectory("editor")
endif()

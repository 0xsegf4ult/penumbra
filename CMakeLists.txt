cmake_minimum_required(VERSION 3.30.5 FATAL_ERROR)

set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_MODULE_STD 1)

project(penumbra VERSION 0.1 LANGUAGES CXX)

include(FetchContent)

option(PENUMBRA_BUILD_TOOLS "Build tools" ON)
option(PENUMBRA_BUILD_EDITOR "Build editor" ON)
option(PENUMBRA_ENABLE_TRACY "Enable Tracy" OFF)

if(NOT ${CMAKE_GENERATOR} MATCHES "Ninja")
	message(FATAL_ERROR "Generator ${CMAKE_GENERATOR} is not supported with C++ modules")
endif()

add_subdirectory("external")
add_subdirectory("modules")

if(PENUMBRA_BUILD_TOOLS)
	add_subdirectory("tools")
endif()

if(PENUMBRA_BUILD_EDITOR)
	add_subdirectory("editor")
endif()

cmake_minimum_required(VERSION 3.30.5 FATAL_ERROR)

set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(penumbra VERSION 0.1 LANGUAGES CXX)

if(NOT ${CMAKE_GENERATOR} MATCHES "Ninja")
	message(FATAL_ERROR "Generator ${CMAKE_GENERATOR} is not supported with C++ modules")
endif()

add_subdirectory("src")

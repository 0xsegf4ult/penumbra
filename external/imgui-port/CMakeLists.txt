cmake_minimum_required(VERSION 3.30.1 FATAL_ERROR)

project(imgui-port VERSION 1.92.5 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

FetchContent_Declare(imgui URL https://github.com/ocornut/imgui/archive/refs/tags/v1.92.5-docking.zip)
FetchContent_MakeAvailable(imgui)

FetchContent_Declare(imguizmo GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo GIT_TAG a15acd87a3f3241a29ea1363ceafc680dca3a96b)
FetchContent_MakeAvailable(imguizmo)

add_library(imgui-port STATIC)
target_include_directories(imgui-port PRIVATE ${imgui_SOURCE_DIR} ${imguizmo_SOURCE_DIR})
target_sources(imgui-port PUBLIC FILE_SET CXX_MODULES
	FILES
	mod.cppm
	PRIVATE
	${imgui_SOURCE_DIR}/imgui.cpp
	${imgui_SOURCE_DIR}/imgui_demo.cpp
	${imgui_SOURCE_DIR}/imgui_draw.cpp
	${imgui_SOURCE_DIR}/imgui_tables.cpp
	${imgui_SOURCE_DIR}/imgui_widgets.cpp
	${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
	${imguizmo_SOURCE_DIR}/ImGuizmo.cpp)

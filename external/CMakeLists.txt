FetchContent_Declare(volk URL https://github.com/zeux/volk/archive/refs/tags/vulkan-sdk-1.4.341.0.zip)
FetchContent_MakeAvailable(volk)

FetchContent_Declare(spdlog URL https://github.com/gabime/spdlog/archive/refs/tags/v1.17.0.zip)
set(SPDLOG_USE_STD_FORMAT ON CACHE BOOL "" FORCE)
set(SPDLOG_NO_EXCEPTIONS ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(spdlog)

add_subdirectory("imgui-port")

FetchContent_Declare(entt URL https://github.com/skypjack/entt/archive/refs/tags/v3.16.0.zip)
FetchContent_MakeAvailable(entt)

FetchContent_Declare(tracy URL https://github.com/wolfpld/tracy/archive/refs/tags/v0.13.1.zip)
set(TRACY_LTO ON CACHE BOOL "" FORCE)
set(TRACY_ENABLE ${PENUMBRA_ENABLE_TRACY})
FetchContent_MakeAvailable(tracy)

if(PENUMBRA_BUILD_EDITOR)
	FetchContent_Declare(fastgltf URL https://github.com/spnda/fastgltf/archive/refs/tags/v0.9.0.zip)
	set(FASTGLTF_ENABLE_CPP_MODULES ON CACHE BOOL "" FORCE)
	set(FASTGLTF_USE_STD_MODULE ON CACHE BOOL "" FORCE)
	FetchContent_MakeAvailable(fastgltf)

	FetchContent_Declare(meshoptimizer URL https://github.com/zeux/meshoptimizer/archive/refs/tags/v1.0.zip)
	FetchContent_MakeAvailable(meshoptimizer)
	
	# FetchContent_Declare(ispc_texcomp GIT_REPOSITORY https://github.com/sarc-acl/ISPCTextureCompressor GIT_TAG d35ab81a5c22924438a90969285b4dcd764bd706)
	FetchContent_Declare(ispc_texcomp GIT_REPOSITORY https://github.com/BeardyKing/ispc_texture_compressor_cmake)
	set(ISPC_COMPILER_LOCATION "ispc" CACHE STRING "" FORCE)
	set(ISPC_COMPILER_TARGET "avx2-i32x8" CACHE STRING "" FORCE)
	FetchContent_MakeAvailable(ispc_texcomp)
endif()

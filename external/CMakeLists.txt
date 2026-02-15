FetchContent_Declare(volk URL https://github.com/zeux/volk/archive/refs/tags/vulkan-sdk-1.4.341.0.zip)
FetchContent_MakeAvailable(volk)

FetchContent_Declare(spdlog URL https://github.com/gabime/spdlog/archive/refs/tags/v1.17.0.zip)
set(SPDLOG_USE_STD_FORMAT ON CACHE BOOL "" FORCE)
set(SPDLOG_NO_EXCEPTIONS ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(spdlog)

add_subdirectory("imgui-port")

FetchContent_Declare(tracy URL https://github.com/wolfpld/tracy/archive/refs/tags/v0.13.1.zip)
set(TRACY_LTO ON CACHE BOOL "" FORCE)
set(TRACY_ENABLE ${PENUMBRA_ENABLE_TRACY})
FetchContent_MakeAvailable(tracy)


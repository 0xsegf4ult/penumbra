FetchContent_Declare(volk URL https://github.com/zeux/volk/archive/refs/tags/vulkan-sdk-1.4.341.0.zip)
FetchContent_MakeAvailable(volk)

FetchContent_Declare(spdlog URL https://github.com/gabime/spdlog/archive/refs/tags/v1.17.0.zip)
set(SPDLOG_USE_STD_FORMAT ON)
set(SPDLOG_NO_EXCEPTIONS ON)
FetchContent_MakeAvailable(spdlog)

add_subdirectory("imgui-port")

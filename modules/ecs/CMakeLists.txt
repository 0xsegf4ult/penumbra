add_library(penumbra_ecs INTERFACE "")

target_link_libraries(penumbra_ecs INTERFACE EnTT::EnTT)
target_include_directories(penumbra_ecs INTERFACE ${CMAKE_SOURCE_DIR}/include)

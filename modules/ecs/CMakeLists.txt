add_library(penumbra_ecs STATIC "")

target_link_libraries(penumbra_ecs PUBLIC EnTT::EnTT)
target_sources(penumbra_ecs PUBLIC FILE_SET CXX_MODULES FILES mod.cppm)

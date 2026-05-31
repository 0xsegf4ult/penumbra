module;

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

export module penumbra.editor:import_gltf;

import penumbra.anim;
import penumbra.core;
import penumbra.math;
import penumbra.resource;
import penumbra.ecs;
import fastgltf;
import std;

import :import_geometry;
import :import_texture;
import :render_object_component;
import :world_state;

using std::uint32_t, std::size_t;

template<>
struct fastgltf::ElementTraits<penumbra::vec2> : fastgltf::ElementTraitsBase<penumbra::vec2, fastgltf::AccessorType::Vec2, float> {};

template<>
struct fastgltf::ElementTraits<penumbra::vec3> : fastgltf::ElementTraitsBase<penumbra::vec3, fastgltf::AccessorType::Vec3, float> {};

template<>
struct fastgltf::ElementTraits<penumbra::vec4> : fastgltf::ElementTraitsBase<penumbra::vec4, fastgltf::AccessorType::Vec4, float> {};

template<>
struct fastgltf::ElementTraits<penumbra::uvec4> : fastgltf::ElementTraitsBase<penumbra::uvec4, fastgltf::AccessorType::Vec4, uint32_t> {};

template<class... T>
struct overloaded : T... { using T::operator()...; };

namespace penumbra
{

struct CachedPrimitive
{
	ResourceID geometry;
	ResourceID material;
};

export struct gltf_import_context
{
	WorldState* world;
	std::map<size_t, std::vector<CachedPrimitive>> mesh_map;
	std::map<size_t, ResourceID> texture_map;
	std::map<size_t, ResourceID> material_map;
	std::map<size_t, ResourceID> skeleton_map;
	std::map<size_t, uint16_t> node_to_bone_map;

	std::atomic<uint32_t> step_counter{0u};
	uint32_t num_steps = 4;
};

void process_skin_joint(gltf_import_context& ctx, Skeleton& skel, fastgltf::Asset& gltf, uint32_t bone, uint32_t parent, uint32_t& sb_index)
{
	auto& node = gltf.nodes[bone];
	ctx.node_to_bone_map[bone] = sb_index;

	auto trs = std::get<fastgltf::TRS>(node.transform);
	
	skel.bone_names[sb_index] = std::string{node.name};
	skel.bone_transforms[sb_index] = Transform{vec3{trs.translation.x(), trs.translation.y(), trs.translation.z()}, Quaternion{trs.rotation.x(), trs.rotation.y(), trs.rotation.z(), trs.rotation.w()}, vec3{trs.scale.x(), trs.scale.y(), trs.scale.z()}};
	skel.bone_parents[sb_index] = parent;
	
	auto self = sb_index;

	for(auto child : node.children)
	{
		sb_index++;
		process_skin_joint(ctx, skel, gltf, child, self + 1, sb_index);
	}
}

ResourceID parse_gltf_skin(gltf_import_context& ctx, fastgltf::Asset& gltf, size_t index)
{
	if(ctx.skeleton_map.contains(index))
		return ctx.skeleton_map[index];

	auto& skin = gltf.skins[index];
	if(skin.skeleton.has_value())
		log::debug("skeleton root node {}", gltf.nodes[skin.skeleton.value()].name);

	Skeleton skel;
	skel.name = std::string{skin.name};
	skel.bone_count = static_cast<uint16_t>(skin.joints.size());
	skel.bone_names.resize(skel.bone_count);
	skel.bone_transforms.resize(skel.bone_count);
	skel.bone_parents.resize(skel.bone_count);
	skel.bone_inv_bind_matrices.resize(skel.bone_count);

	std::vector<uint32_t> parent_table(skel.bone_count);

	uint32_t sb_index = 0;
	process_skin_joint(ctx, skel, gltf, skin.joints[0], 0, sb_index);

	if(!skin.inverseBindMatrices.has_value())
	{
		log::warn("gltf_import: skin {} does not have inverse bind matrices", skin.name);
		return ResourceID{};
	}

	auto inv_bind = skin.inverseBindMatrices.value();
	auto& inv_bind_accessor = gltf.accessors[inv_bind];
	fastgltf::iterateAccessorWithIndex<fastgltf::math::fmat4x4>(gltf, inv_bind_accessor, [&skel](fastgltf::math::fmat4x4 data, size_t idx)
	{
		memcpy(&skel.bone_inv_bind_matrices[idx], data.data(), sizeof(mat4));
	});

	ResourceID rid = resource_manager_import_skeleton(skel);
	ctx.skeleton_map[index] = rid;
	return rid;
}

ResourceID get_cached_texture(gltf_import_context& ctx, fastgltf::Asset& gltf, fastgltf::Texture& texture, size_t material, import_texture_type type)
{
	if(ctx.texture_map.contains(texture.imageIndex.value()))
		return ctx.texture_map[texture.imageIndex.value()];

	auto& image = gltf.images[texture.imageIndex.value()];
	ResourceID rid{};

	std::string texname;
	if(image.name.length() > 0)
		texname = std::string{image.name};
	else
		texname = std::format("{}_{}", gltf.materials[material].name, texture_type_names[std::to_underlying(type)]);

	std::visit([&](auto&& arg)
	{
		using T = std::decay_t<decltype(arg)>;
		if constexpr (std::is_same_v<T, fastgltf::sources::BufferView>)
		{
			auto& bufferView = gltf.bufferViews[arg.bufferViewIndex];
			auto& buffer = gltf.buffers[bufferView.bufferIndex];
			std::visit([&](auto&& source)
			{
				using P = std::decay_t<decltype(source)>;
				if constexpr (std::is_same_v<P, fastgltf::sources::Array>)
				{
					int w, h, c;
					unsigned char* data = stbi_load_from_memory(reinterpret_cast<unsigned char*>(source.bytes.data()) + bufferView.byteOffset, static_cast<int>(bufferView.byteLength), &w, &h, &c, 0);
					log::debug("get_cached_texture {} {} {}x{} {}bpp", uint32_t(arg.mimeType), texname, w, h, c * 8);
					if(data)
					{
						rid = import_texture(texname, type, {reinterpret_cast<const std::byte*>(data), size_t(w * h * c)}, uvec3{uint32_t(w), uint32_t(h), uint32_t(c)});
						stbi_image_free(data);
					}
				}
			}, buffer.data);
		}
	}, image.data);
	
	if(rid.get_handle())
		ctx.texture_map[texture.imageIndex.value()] = rid;
	else
		log::warn("import_gtlf: failed to load texture {}", texname);

	return rid;
}

float ior_to_reflectivity(float ior)
{
	float ref_pct = ((ior - 1.0) * (ior - 1.0)) / ((ior + 1.0) * (ior + 1.0));
	return std::sqrtf(ref_pct / 0.16);
}

ResourceID get_cached_material(gltf_import_context& ctx, fastgltf::Asset& gltf, size_t index)
{
	if(ctx.material_map.contains(index))
		return ctx.material_map[index];

	auto& material = gltf.materials[index];

	uint32_t mtl_flags{0u};
	if(material.alphaMode == fastgltf::AlphaMode::Mask)
		mtl_flags |= RENDER_MATERIAL_ALPHA_MASK;
	else if(material.alphaMode == fastgltf::AlphaMode::Blend)
		mtl_flags |= RENDER_MATERIAL_ALPHA_BLEND;

	if(material.doubleSided)
		mtl_flags |= RENDER_MATERIAL_DOUBLESIDED;

	ResourceID albedo{};
	if(material.pbrData.baseColorTexture.has_value())
	{
		auto& texture = gltf.textures[material.pbrData.baseColorTexture.value().textureIndex];
		albedo = get_cached_texture(ctx, gltf, texture, index, import_texture_type::albedo);
	}
	ResourceID mro{};
	if(material.pbrData.metallicRoughnessTexture.has_value())
	{
		auto& texture = gltf.textures[material.pbrData.metallicRoughnessTexture.value().textureIndex];
		mro = get_cached_texture(ctx, gltf, texture, index, import_texture_type::mro);
	}
	ResourceID normalmap{};
	if(material.normalTexture.has_value())
	{
		auto& texture = gltf.textures[material.normalTexture.value().textureIndex];
		normalmap = get_cached_texture(ctx, gltf, texture, index, import_texture_type::normalmap);
	}
	ResourceID emissive{};
	if(material.emissiveTexture.has_value())
	{
		auto& texture = gltf.textures[material.emissiveTexture.value().textureIndex];
		emissive = get_cached_texture(ctx, gltf, texture, index, import_texture_type::emissive);
	}

	if(material.anisotropy.get())
	{
		log::info("gltf_import: material {} ANISOTROPIC", material.name);
	}

	

	ClearcoatMaterialInfo clearcoat_info{};

	if(material.clearcoat.get())
	{
		log::info("gltf_import: material {} CLEARCOAT", material.name);
		clearcoat_info.factor = material.clearcoat->clearcoatFactor;
		clearcoat_info.roughness_factor = material.clearcoat->clearcoatRoughnessFactor;
		if(material.clearcoat->clearcoatTexture.has_value())
			log::debug("has clearcoat texture");
		if(material.clearcoat->clearcoatRoughnessTexture.has_value())
			log::debug("has clearcoat roughness texture");
		if(material.clearcoat->clearcoatNormalTexture.has_value())
			log::debug("has clearcoat normal texture");

		mtl_flags |= RENDER_MATERIAL_CLEARCOAT;
	}

	auto rid = resource_manager_create_material
	(MaterialResource{
		.name{material.name},
		.factors = 
		{
			.albedo = vec4{material.pbrData.baseColorFactor.x(), material.pbrData.baseColorFactor.y(), material.pbrData.baseColorFactor.z(), material.pbrData.baseColorFactor.w()},
			.roughness = material.pbrData.roughnessFactor,
			.metallic = material.pbrData.metallicFactor,
			.normal = material.normalTexture.has_value() ? material.normalTexture.value().scale : 1.0f,
			.reflectivity = ior_to_reflectivity(material.ior),
			.alpha_cf = material.alphaCutoff,
			.emissive = vec3{material.emissiveFactor.x(), material.emissiveFactor.y(), material.emissiveFactor.z()} * material.emissiveStrength
		},
		.flags = mtl_flags,
		.albedo = albedo,
		.mro = mro,
		.normalmap = normalmap,
		.emissive = emissive,
		.clearcoat = clearcoat_info
	});
	ctx.material_map[index] = rid;
	return rid;
}

std::vector<CachedPrimitive>& get_cached_mesh(gltf_import_context& ctx, fastgltf::Asset& gltf, size_t index)
{
	if(ctx.mesh_map.contains(index))
		return ctx.mesh_map[index];

	ctx.mesh_map[index] = {};
	auto& mesh = gltf.meshes[index];

	uint32_t prim_index = 0;
	for(auto& prim : mesh.primitives)
	{
		geometry_import_context geo_import{};
		if(mesh.primitives.size() == 1)
			geo_import.name = std::string{mesh.name};
		else
			geo_import.name = std::format("{}_prim{}", mesh.name, prim_index);

		if(prim.indicesAccessor.has_value())
		{
			auto& ind_accessor = gltf.accessors[prim.indicesAccessor.value()];
		       	geo_import.indices.resize(ind_accessor.count);
			fastgltf::copyFromAccessor<uint32_t>(gltf, ind_accessor, geo_import.indices.data());	
		}

		auto* pos_attr = prim.findAttribute("POSITION");
		auto& pos_accessor = gltf.accessors[pos_attr->accessorIndex]; 
		geo_import.vertices.resize(pos_accessor.count);
		fastgltf::iterateAccessorWithIndex<vec3>(gltf, pos_accessor, [&geo_import](vec3 data, size_t idx)
		{
			geo_import.vertices[idx].pos = data;
		});

		auto* uv_attr = prim.findAttribute("TEXCOORD_0");
		if(uv_attr != prim.attributes.end())
		{
			auto& uv_accessor = gltf.accessors[uv_attr->accessorIndex];
			fastgltf::iterateAccessorWithIndex<vec2>(gltf, uv_accessor, [&geo_import](vec2 data, size_t idx)
			{
				geo_import.vertices[idx].uv = data;
			});
		}

		auto* nrm_attr = prim.findAttribute("NORMAL");
		if(nrm_attr != prim.attributes.end())
		{
			auto& nrm_accessor = gltf.accessors[nrm_attr->accessorIndex];
			fastgltf::iterateAccessorWithIndex<vec3>(gltf, nrm_accessor, [&geo_import](vec3 data, size_t idx)
			{
				geo_import.vertices[idx].nrm = data;
			});
			geo_import.has_normals = true;
		}

		auto* tan_attr = prim.findAttribute("TANGENT");
		if(tan_attr != prim.attributes.end())
		{
			auto& tan_accessor = gltf.accessors[tan_attr->accessorIndex];
			fastgltf::iterateAccessorWithIndex<vec4>(gltf, tan_accessor, [&geo_import](vec4 data, size_t idx)
			{
				geo_import.vertices[idx].tan = data;
			});
			geo_import.has_tangents = true;
		}		

		auto* joint_attr = prim.findAttribute("JOINTS_0");
	       	if(joint_attr != prim.attributes.end())
		{
			auto& joint_accessor = gltf.accessors[joint_attr->accessorIndex];
			fastgltf::iterateAccessorWithIndex<uvec4>(gltf, joint_accessor, [&geo_import](uvec4 data, size_t idx)
			{
				geo_import.vertices[idx].joints = data;
			});
			geo_import.is_skinned = true;
		}

		auto* weight_attr = prim.findAttribute("WEIGHTS_0");
		if(weight_attr != prim.attributes.end())
		{
			auto& weight_accessor = gltf.accessors[weight_attr->accessorIndex];
			fastgltf::iterateAccessorWithIndex<vec4>(gltf, weight_accessor, [&geo_import](vec4 data, size_t idx)
			{
				geo_import.vertices[idx].weights = data;
			});
			geo_import.is_skinned = true;
		}	

		auto geom = import_geometry(geo_import);
		ResourceID material{};

		if(prim.materialIndex.has_value())
			material = get_cached_material(ctx, gltf, prim.materialIndex.value());
		
		ctx.mesh_map[index].emplace_back(geom, material);

		prim_index++;
	}

	return ctx.mesh_map[index];
}

RenderBucket determine_material_bucket(ResourceID material)
{
	if(!material.get_handle())
		return RENDER_BUCKET_DEFAULT;

	auto mtl_flags = resource_manager_get_material(material).flags;

	if(mtl_flags & RENDER_MATERIAL_ALPHA_MASK)
	{
		if(mtl_flags & RENDER_MATERIAL_DOUBLESIDED)
			return RENDER_BUCKET_ALPHA_MASKED_DOUBLE_SIDED;

		return RENDER_BUCKET_ALPHA_MASKED;
	}

	if(mtl_flags & RENDER_MATERIAL_ALPHA_BLEND)
	{
		if(mtl_flags & RENDER_MATERIAL_DOUBLESIDED)
			return RENDER_BUCKET_TRANSPARENT_DOUBLE_SIDED;

		return RENDER_BUCKET_TRANSPARENT;
	}

	if(mtl_flags & RENDER_MATERIAL_DOUBLESIDED)
		return RENDER_BUCKET_DOUBLE_SIDED;

	return RENDER_BUCKET_DEFAULT;
}

void parse_gltf_mesh(gltf_import_context& ctx, fastgltf::Asset& gltf, size_t index, ecs::entity parent, mat4 matrix_world, ResourceID skeleton = ResourceID{})
{
	auto& primitives = get_cached_mesh(ctx, gltf, index);

	auto pcount = primitives.size();
	if(pcount == 1 && primitives[0].geometry.get_handle())
	{
		auto& geom_data = resource_manager_get_geometry(primitives[0].geometry);
		auto material = primitives[0].material;
		
		auto vtx_offset = geom_data.vertex_offset;
		if(geom_data.skinned_vertex && skeleton.get_handle())
		{
			auto sg_instance = renderer_skinned_geometry_instantiate(vtx_offset, geom_data.vertex_count, resource_manager_get_skeleton(skeleton).bone_count);
			vtx_offset = renderer_get_skinned_geometry_vertices(sg_instance);
			ctx.world->entities.emplace<render_anim_component>(parent, skeleton, sg_instance);
		}

		auto rd_object = renderer_world_insert_object
		({
			matrix_world,
		 	determine_material_bucket(material),
			geom_data.sphere,
			material.get_handle(),
			geom_data.l0_cluster_count,
			geom_data.lod_offset,
			geom_data.lod_count,
			vtx_offset,
			geom_data.index_offset,
			geom_data.cluster_offset
		}, 4);

		ctx.world->entities.emplace<render_object_component>(parent, primitives[0].geometry, material, rd_object);
		return;
	}

	for(size_t i = 0; i < pcount; i++)
	{
		Transform ntx = ctx.world->entities.get<Transform>(parent);
		auto p_ent = ctx.world->spawn(std::format("prim{}", i));
		add_entity_as_child(ctx.world->entities, parent, p_ent);
		ctx.world->entities.emplace<Transform>(p_ent, ntx);
	
		if(!primitives[i].geometry.get_handle())
			continue;

		auto& geom_data = resource_manager_get_geometry(primitives[i].geometry);
		auto material = primitives[i].material;
	
		auto vtx_offset = geom_data.vertex_offset;
		if(geom_data.skinned_vertex && skeleton.get_handle())
		{
			auto sg_instance = renderer_skinned_geometry_instantiate(vtx_offset, geom_data.vertex_count, resource_manager_get_skeleton(skeleton).bone_count);
			vtx_offset = renderer_get_skinned_geometry_vertices(sg_instance);
			ctx.world->entities.emplace<render_anim_component>(p_ent, skeleton, sg_instance);
		}

		auto rd_object = renderer_world_insert_object
		({
		 	matrix_world,
		 	determine_material_bucket(material),
			geom_data.sphere,
			material.get_handle(),
			geom_data.l0_cluster_count,
			geom_data.lod_offset,
			geom_data.lod_count,
			vtx_offset,
			geom_data.index_offset,
			geom_data.cluster_offset
		}, 4);

		ctx.world->entities.emplace<render_object_component>(p_ent, primitives[i].geometry, material, rd_object);
	}
}

void parse_gltf_node(gltf_import_context& ctx, fastgltf::Asset& gltf, size_t index, ecs::entity parent, mat4 parent_matrix_world)
{
	fastgltf::Node& node = gltf.nodes[index];
	ecs::entity entity = ctx.world->spawn(node.name);
	add_entity_as_child(ctx.world->entities, parent, entity);

	auto trs = std::get<fastgltf::TRS>(node.transform);

	bool has_skin = node.skinIndex.has_value();
	Transform ntx{};
	if(!has_skin)
	{
		ntx = Transform
		{
			vec3{trs.translation.x(), trs.translation.y(), trs.translation.z()},
			Quaternion{trs.rotation.x(), trs.rotation.y(), trs.rotation.z(), trs.rotation.w()},
			vec3{trs.scale.x(), trs.scale.y(), trs.scale.z()}
		};
	}

	ctx.world->entities.emplace<Transform>(entity, ntx);
	mat4 matrix_world = ntx.as_matrix() * parent_matrix_world;

	ResourceID skeleton{};
	if(has_skin)
	{
		skeleton = parse_gltf_skin(ctx, gltf, node.skinIndex.value());
	}

	if(node.meshIndex.has_value())
	{
		parse_gltf_mesh(ctx, gltf, node.meshIndex.value(), entity, matrix_world, skeleton);
	}

	for(size_t child : node.children)
	{
		parse_gltf_node(ctx, gltf, child, entity, matrix_world);
	}
}

void parse_gltf_animation(gltf_import_context& ctx, fastgltf::Asset& gltf, fastgltf::Animation& anim)
{
	Animation res;
	res.name = std::string(anim.name);
	res.channels.resize(anim.channels.size());

	uint32_t chn_index = 0;
	for(auto& channel : anim.channels)
	{
		auto& res_chn = res.channels[chn_index];

		if(!channel.nodeIndex.has_value())
			continue;

		auto node_index = channel.nodeIndex.value();
		if(!ctx.node_to_bone_map.contains(node_index))
		{
			log::warn("gltf_import: animation {} channel points to invalid node {}", anim.name, gltf.nodes[node_index].name);
			continue;	
		}

		res_chn.bone = ctx.node_to_bone_map[node_index];

		auto& sampler = anim.samplers[channel.samplerIndex];
		
		switch(sampler.interpolation)
		{
		case fastgltf::AnimationInterpolation::Linear:
			res_chn.interp = AnimationInterp::Linear;
			break;
		case fastgltf::AnimationInterpolation::Step:
			res_chn.interp = AnimationInterp::Constant;
			break;
		case fastgltf::AnimationInterpolation::CubicSpline:
			res_chn.interp = AnimationInterp::CubicSpline;
			break;
		default:
			log::warn("gltf_import: animation {} channel has invalid interp", anim.name);
			continue;
		}
		
		auto& time_accessor = gltf.accessors[sampler.inputAccessor];
		res_chn.timestamps.resize(time_accessor.count);
		fastgltf::iterateAccessorWithIndex<float>(gltf, time_accessor, [&res, &res_chn](float data, size_t idx)
		{
			res_chn.timestamps[idx] = data;
			res.start_time = std::min(res.start_time, data);
			res.end_time = std::max(res.end_time, data);
		});

		auto& data_accessor = gltf.accessors[sampler.outputAccessor];
		
		switch(channel.path)
		{
		case fastgltf::AnimationPath::Translation:
			res_chn.path = AnimationPath::Translation;
			res_chn.values.resize(sizeof(vec3) * data_accessor.count);
			fastgltf::iterateAccessorWithIndex<vec3>(gltf, data_accessor, [&res_chn](vec3 data, size_t idx)
			{
				memcpy(res_chn.values.data() + (idx * 3), &data, sizeof(vec3));
			});
			break;
		case fastgltf::AnimationPath::Rotation:
			res_chn.path = AnimationPath::Rotation;
			res_chn.values.resize(sizeof(Quaternion) * data_accessor.count);
			fastgltf::iterateAccessorWithIndex<vec4>(gltf, data_accessor, [&res_chn](vec4 data,  size_t idx)
			{
				memcpy(res_chn.values.data() + (idx * 4), &data, sizeof(vec4));
			});
			break;
		case fastgltf::AnimationPath::Scale:
			res_chn.path = AnimationPath::Scale;
			res_chn.values.resize(sizeof(vec3) * data_accessor.count);
			fastgltf::iterateAccessorWithIndex<vec3>(gltf, data_accessor, [&res_chn](vec3 data, size_t idx)
			{
				memcpy(res_chn.values.data() + (idx * 3), &data, sizeof(vec3));
			});
			break;
		default:
			log::warn("gltf_import: animation {} channel has invalid path", anim.name);
			continue;
		}
		
		chn_index++;
	}

	resource_manager_import_animation(res);
}

void parse_gltf(gltf_import_context& ctx, fastgltf::Asset& gltf)
{
	for(auto ext : gltf.extensionsUsed)
	{
		log::info("parse_gltf: using extension {}", ext);
	}

	size_t default_scene = 0;
	if(gltf.defaultScene.has_value())
		default_scene = gltf.defaultScene.value();

	fastgltf::Scene& scene = gltf.scenes[default_scene];

	for(size_t node_index : scene.nodeIndices)
	{
		parse_gltf_node(ctx, gltf, node_index, ctx.world->root, mat4::identity());
	}

	for(auto& anim : gltf.animations)
	{
		parse_gltf_animation(ctx, gltf, anim);
	}
}

export bool import_gltf(gltf_import_context& ctx, const vfs::path& path)
{
	fastgltf::Parser parser(fastgltf::Extensions::KHR_materials_ior | fastgltf::Extensions::KHR_materials_emissive_strength | fastgltf::Extensions::KHR_materials_clearcoat);
	auto data = fastgltf::GltfDataBuffer::FromPath(path);
	if(data.error() != fastgltf::Error::None)
	{
		log::warn("import_gltf failed: {}", fastgltf::getErrorMessage(data.error()));
		return false;
	}

	auto gltf = parser.loadGltf(data.get(), path.parent_path(), fastgltf::Options::DecomposeNodeMatrices);
	if(gltf.error() != fastgltf::Error::None)
	{
		log::warn("import_gltf failed: {}", fastgltf::getErrorMessage(gltf.error()));
		return false;
	}

	parse_gltf(ctx, gltf.get());

	return true;
}

}

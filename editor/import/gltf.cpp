#include <import/gltf.hpp>
#include <import/resource.hpp>

#include <penumbra/math/transform.hpp>
#include <penumbra/ecs.hpp>
#include <penumbra/resource.hpp>
#include <penumbra/renderer.hpp>
#include <penumbra/log.hpp>
#include <penumbra/types.hpp>

#include <world/state.hpp>
#include <world/components/render.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>

#include <format>
#include <map>
#include <string>
#include <utility>
#include <variant>
#include <vector>

using std::memcpy;

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

/*
static void process_skin_joint(gltf_import_context& ctx, Skeleton& skel, fastgltf::Asset& gltf, u32 bone, u32 parent, u32& sb_index)
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

static ResourceID parse_gltf_skin(gltf_import_context& ctx, fastgltf::Asset& gltf, size_t index)
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

	std::vector<u32> parent_table(skel.bone_count);

	u32 sb_index = 0;
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
*/
static ResourceID get_cached_texture(gltf_import_context& ctx, fastgltf::Asset& gltf, fastgltf::Texture& texture, size_t material, import_texture_type type)
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
						rid = import_texture(texname, type, {reinterpret_cast<const u8*>(data), size_t(w * h * c)}, uvec3{u32(w), u32(h), u32(c)});
						stbi_image_free(data);
					}
				}
			}, buffer.data);
		}
	}, image.data);
	
	if(resource_get_handle(rid))
		ctx.texture_map[texture.imageIndex.value()] = rid;
	else
		log::warn("import_gtlf: failed to load texture {}", texname);

	return rid;
}

static float ior_to_reflectivity(float ior)
{
	float ref_pct = ((ior - 1.0) * (ior - 1.0)) / ((ior + 1.0) * (ior + 1.0));
	return std::sqrtf(ref_pct / 0.16);
}

static ResourceID get_cached_material(gltf_import_context& ctx, fastgltf::Asset& gltf, size_t index)
{
	if(ctx.material_map.contains(index))
		return ctx.material_map[index];

	auto& material = gltf.materials[index];

	u32 mtl_flags{0u};
	if(material.alphaMode == fastgltf::AlphaMode::Mask)
		mtl_flags |= MATERIAL_ALPHA_MASK;
	else if(material.alphaMode == fastgltf::AlphaMode::Blend)
		mtl_flags |= MATERIAL_ALPHA_BLEND;

	if(material.doubleSided)
		mtl_flags |= MATERIAL_DOUBLE_SIDED;

	ResourceID albedo{};
	if(material.pbrData.baseColorTexture.has_value())
	{
		auto& texture = gltf.textures[material.pbrData.baseColorTexture.value().textureIndex];
		albedo = get_cached_texture(ctx, gltf, texture, index, IMPORT_TEXTURE_ALBEDO);
	}
	ResourceID mro{};
	if(material.pbrData.metallicRoughnessTexture.has_value())
	{
		auto& texture = gltf.textures[material.pbrData.metallicRoughnessTexture.value().textureIndex];
		mro = get_cached_texture(ctx, gltf, texture, index, IMPORT_TEXTURE_MRO);
	}
	ResourceID normalmap{};
	if(material.normalTexture.has_value())
	{
		auto& texture = gltf.textures[material.normalTexture.value().textureIndex];
		normalmap = get_cached_texture(ctx, gltf, texture, index, IMPORT_TEXTURE_NORMALMAP);
	}
	ResourceID emissive{};
	if(material.emissiveTexture.has_value())
	{
		auto& texture = gltf.textures[material.emissiveTexture.value().textureIndex];
		emissive = get_cached_texture(ctx, gltf, texture, index, IMPORT_TEXTURE_EMISSIVE);
	}

	if(material.anisotropy.get())
	{
		log::info("gltf_import: material {} ANISOTROPIC", material.name);
	}

	clearcoat_info clearcoat_desc{};

	if(material.clearcoat.get())
	{
		log::info("gltf_import: material {} CLEARCOAT", material.name);
		clearcoat_desc.factor = material.clearcoat->clearcoatFactor;
		clearcoat_desc.roughness_factor = material.clearcoat->clearcoatRoughnessFactor;
		if(material.clearcoat->clearcoatTexture.has_value())
			log::debug("has clearcoat texture");
		if(material.clearcoat->clearcoatRoughnessTexture.has_value())
			log::debug("has clearcoat roughness texture");
		if(material.clearcoat->clearcoatNormalTexture.has_value())
			log::debug("has clearcoat normal texture");

		mtl_flags |= MATERIAL_CLEARCOAT;
	}

	auto rid = resource_manager_import_material
	(material_resource{
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
		.clearcoat = clearcoat_desc
	});
	ctx.material_map[index] = rid;
	return rid;
}

static std::vector<CachedPrimitive>& get_cached_mesh(gltf_import_context& ctx, fastgltf::Asset& gltf, size_t index)
{
	if(ctx.mesh_map.contains(index))
		return ctx.mesh_map[index];

	ctx.mesh_map[index] = {};
	auto& mesh = gltf.meshes[index];

	u32 prim_index = 0;
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
			fastgltf::copyFromAccessor<u32>(gltf, ind_accessor, geo_import.indices.data());	
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

static void parse_gltf_mesh(gltf_import_context& ctx, fastgltf::Asset& gltf, size_t index, ecs::entity parent, mat4 matrix_world, ResourceID skeleton = ResourceID{})
{
	auto& primitives = get_cached_mesh(ctx, gltf, index);

	auto pcount = primitives.size();
	if(pcount == 1 && resource_get_handle(primitives[0].geometry))
	{
		auto rd_object = renderer_world_insert_object
		({
			matrix_world,
			primitives[0].geometry,
		 	primitives[0].material,
			skeleton,
		}, 3);
		
		if(resource_get_handle(skeleton))
			ctx.world->entities.emplace<render_skeleton_component>(parent, skeleton);

		ctx.world->entities.emplace<render_object_component>(parent, primitives[0].geometry, primitives[0].material, rd_object);
		return;
	}

	for(size_t i = 0; i < pcount; i++)
	{
		Transform ntx = ctx.world->entities.get<Transform>(parent);
		auto p_ent = ctx.world->spawn(std::format("prim{}", i));
		add_entity_as_child(ctx.world->entities, parent, p_ent);
		ctx.world->entities.emplace_or_replace<Transform>(p_ent, ntx);
	
		if(!resource_get_handle(primitives[i].geometry))
			continue;

		auto rd_object = renderer_world_insert_object
		({
		 	matrix_world,
			primitives[i].geometry,
		 	primitives[i].material,
			skeleton
		}, 3);

		if(resource_get_handle(skeleton))
			ctx.world->entities.emplace<render_skeleton_component>(p_ent, skeleton);

		ctx.world->entities.emplace<render_object_component>(p_ent, primitives[i].geometry, primitives[i].material, rd_object);
	}
}

static void parse_gltf_node(gltf_import_context& ctx, fastgltf::Asset& gltf, size_t index, ecs::entity parent, mat4 parent_matrix_world)
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

	ctx.world->entities.emplace_or_replace<Transform>(entity, ntx);
	mat4 matrix_world = ntx.as_matrix() * parent_matrix_world;

	ResourceID skeleton{};
/*	if(has_skin)
	{
		skeleton = parse_gltf_skin(ctx, gltf, node.skinIndex.value());
	}*/

	if(node.meshIndex.has_value())
	{
		parse_gltf_mesh(ctx, gltf, node.meshIndex.value(), entity, matrix_world, skeleton);
	}

	for(size_t child : node.children)
	{
		parse_gltf_node(ctx, gltf, child, entity, matrix_world);
	}
}
/*
static void parse_gltf_animation(gltf_import_context& ctx, fastgltf::Asset& gltf, fastgltf::Animation& anim)
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
*/
static void parse_gltf(gltf_import_context& ctx, fastgltf::Asset& gltf)
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

/*	for(auto& anim : gltf.animations)
	{
		parse_gltf_animation(ctx, gltf, anim);
	}*/
}

bool import_gltf(gltf_import_context& ctx, const vfs_path& path)
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

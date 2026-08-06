module;

#include <cassert>

module penumbra.physics:gjk;

import penumbra.core;
import penumbra.math;
import :api;
import :shape;
import std;

using std::uint32_t, std::size_t;

namespace penumbra
{

struct bary_vec2
{
	float u;
	float v;
	float divisor;
};

struct bary_vec3
{
	float u;
	float v;
	float p;
	float divisor;
};

struct bary_vec4
{
	float u;
	float v;
	float p;
	float q;
	float divisor;
};

bary_vec2 line_segment_to_barycentric(vec3 a, vec3 b)
{
	const vec3 ab = b - a;
	const float lensq = ab.magnitude_sqr();

	return {vec3::dot(b, ab), -vec3::dot(a, ab), lensq};
}

bary_vec3 triangle_to_barycentric(vec3 a, vec3 b, vec3 c)
{
	const vec3 ab = b - a;
	const vec3 ac = c - a;

	const vec3 bXc = vec3::cross(b, c);
	const vec3 cXa = vec3::cross(c, a);
	const vec3 aXb = vec3::cross(a, b);

	vec3 abXac = vec3::cross(ab, ac);

	float divisor = abXac.magnitude_sqr();

	return {vec3::dot(bXc, abXac), vec3::dot(cXa, abXac), vec3::dot(aXb, abXac), divisor};
}

float scalar_triple_product(vec3 a, vec3 b, vec3 c)
{
	vec3 d;
	d.x = b.y * c.z - b.z * c.y;
	d.y = b.z * c.x - b.x * c.z;
	d.z = b.x * c.y - b.y * c.x;
	return a.x * d.x + a.y * d.y + a.z * d.z;
}

bary_vec4 tetrahedron_to_barycentric(vec3 a, vec3 b, vec3 c, vec3 d)
{
	const vec3 ab = b - a;
	const vec3 ac = c - a;
	const vec3 ad = d - a;

	float divisor = scalar_triple_product(ab, ac, ad);

	float sign = divisor < 0.0f ? -1.0f : 1.0f;
	return
	{
		sign * scalar_triple_product(b, c, d),
		sign * scalar_triple_product(a, d, c),
		sign * scalar_triple_product(a, b, d),
		sign * scalar_triple_product(a, c, b),
		sign * divisor
	};
}

struct simplex_vertex
{
	size_t indexA;
	size_t indexB;
	vec3 wA;
	vec3 wB;
	vec3 w;
	float a;
};

struct simplex_t
{
	simplex_vertex vertices[4];
	int count;
};

bool simplex_solve2(simplex_t& simplex)
{
	assert(simplex.count == 2);
	auto bary = line_segment_to_barycentric(simplex.vertices[0].w, simplex.vertices[1].w);

	// region A	
	if(bary.v <= 0.0f)
	{
		simplex.count = 1;
		simplex.vertices[0].a = 1.0f;
		return true;
	}

	// region B
	if(bary.u <= 0.0f)
	{
		simplex.count = 1;
		simplex.vertices[0] = simplex.vertices[1];
		simplex.vertices[0].a = 1.0f;

		return true;
	}

	// region AB
	if(bary.divisor <= 0.0f)
		return false;

	simplex.vertices[0].a = bary.u / bary.divisor;
	simplex.vertices[1].a = bary.v / bary.divisor;

	return true;	
}

bool simplex_solve3(simplex_t& simplex)
{
	assert(simplex.count == 3);
	simplex_vertex v1 = simplex.vertices[0];
	simplex_vertex v2 = simplex.vertices[1];
	simplex_vertex v3 = simplex.vertices[2];

	auto wAB = line_segment_to_barycentric(v1.w, v2.w);
	auto wBC = line_segment_to_barycentric(v2.w, v3.w);
	auto wCA = line_segment_to_barycentric(v3.w, v1.w);

	//region A
	if(wAB.v <= 0.0f && wCA.u <= 0.0f)
	{
		simplex.count = 1;
		simplex.vertices[0] = v1;
		simplex.vertices[0].a = 1.0f;
		return true;
	}

	// region B
	if(wBC.v <= 0.0f && wAB.u <= 0.0f)
	{
		simplex.count = 1;
		simplex.vertices[0] = v2;
		simplex.vertices[0].a = 1.0f;
		return true;
	}

	// region C
	if(wCA.v <= 0.0f && wBC.u <= 0.0f)
	{
		simplex.count = 1;
		simplex.vertices[0] = v3;
		simplex.vertices[0].a = 1.0f;
		return true;
	}

	// region AB
	auto wABC = triangle_to_barycentric(v1.w, v2.w, v3.w);
	if(wABC.p <= 0.0f && wAB.u > 0.0f && wAB.v > 0.0f)
	{
		simplex.count = 2;
		simplex.vertices[0] = v1;
		simplex.vertices[1] = v2;

		log::debug("solve3: reduce to AB");
		if(wAB.divisor <= 0.0f)
			return false;

		simplex.vertices[0].a = wAB.u / wAB.divisor;
		simplex.vertices[1].a = wAB.v / wAB.divisor;
		return true;
	}

	// region BC
	if(wABC.u <= 0.0f && wBC.u > 0.0f && wBC.v > 0.0f)
	{
		simplex.count = 2;
		simplex.vertices[0] = v2;
		simplex.vertices[1] = v3;

		log::debug("solve3: reduce to BC");
		if(wBC.divisor <= 0.0f)
			return false;

		simplex.vertices[0].a = wBC.u / wBC.divisor;
		simplex.vertices[1].a = wBC.v / wBC.divisor;
		return true;
	}

	// region CA
	if(wABC.v <= 0.0f && wCA.u > 0.0f && wCA.v > 0.0f)
	{
		simplex.count = 2;
		simplex.vertices[0] = v3;
		simplex.vertices[1] = v1;

		log::debug("solve3: reduce to CA");
		if(wCA.divisor <= 0.0f)
			return false;

		simplex.vertices[0].a = wCA.u / wCA.divisor;
		simplex.vertices[1].a = wCA.v / wCA.divisor;
		return true;
	}

	// region ABC
	if(wABC.divisor <= 0.0f)
	{
		log::debug("solve3: triangle has no area");
		return false;
	}

	simplex.vertices[0].a = wABC.u / wABC.divisor;
	simplex.vertices[1].a = wABC.v / wABC.divisor;
	simplex.vertices[2].a = wABC.p / wABC.divisor;
	return true;
}

bool simplex_solve4(simplex_t& simplex)
{
	assert(simplex.count == 4);
	simplex_vertex v1 = simplex.vertices[0];
	simplex_vertex v2 = simplex.vertices[1];
	simplex_vertex v3 = simplex.vertices[2];
	simplex_vertex v4 = simplex.vertices[3];

	auto wAB = line_segment_to_barycentric(v1.w, v2.w);
	auto wAC = line_segment_to_barycentric(v1.w, v3.w);
	auto wAD = line_segment_to_barycentric(v1.w, v4.w);
	auto wBC = line_segment_to_barycentric(v2.w, v3.w);
	auto wCD = line_segment_to_barycentric(v3.w, v4.w);
	auto wDB = line_segment_to_barycentric(v4.w, v2.w);

	// region A
	if(wAB.v <= 0.0f && wAC.v <= 0.0f && wAD.v <= 0.0f)
	{
		simplex.count = 1;
		simplex.vertices[0] = v1;
		simplex.vertices[0].a = 1.0f;
		return true;
	}

	// region B
	if(wAB.u <= 0.0f && wDB.u <= 0.0f && wBC.v <= 0.0f)
	{
		simplex.count = 1;
		simplex.vertices[0] = v2;
		simplex.vertices[0].a = 1.0f;
		return true;
	}

	// region C
	if(wAC.u <= 0.0f && wBC.u <= 0.0f && wCD.v <= 0.0f)
	{
		simplex.count = 1;
		simplex.vertices[0] = v3;
		simplex.vertices[0].a = 1.0f;
		return true;
	}

	// region D
	if(wAD.u <= 0.0f && wCD.u <= 0.0f && wDB.v <= 0.0f)
	{
		simplex.count = 1;
		simplex.vertices[0] = v4;
		simplex.vertices[0].a = 1.0f;
		return true;
	}

	auto wACB = triangle_to_barycentric(v1.w, v3.w, v2.w);
	auto wABD = triangle_to_barycentric(v1.w, v2.w, v4.w);
	auto wADC = triangle_to_barycentric(v1.w, v4.w, v3.w);
	auto wBCD = triangle_to_barycentric(v2.w, v3.w, v4.w);

	// region AB
	if(wABD.p <= 0.0f && wACB.v <= 0.0f && wAB.u > 0.0f && wAB.v > 0.0f)
	{
		simplex.count = 2;
		simplex.vertices[0] = v1;
		simplex.vertices[1] = v2;

		if(wAB.divisor <= 0.0f)
			return false;

		simplex.vertices[0].a = wAB.u / wAB.divisor;
		simplex.vertices[1].a = wAB.v / wAB.divisor;
		return true;
	}

	// region AC
	if(wACB.p <= 0.0f && wADC.v <= 0.0f && wAC.u > 0.0f && wAC.v > 0.0f)
	{
		simplex.count = 2;
		simplex.vertices[0] = v1;
		simplex.vertices[1] = v3;

		if(wAC.divisor <= 0.0f)
			return false;

		simplex.vertices[0].a = wAC.u / wAC.divisor;
		simplex.vertices[1].a = wAC.v / wAC.divisor;
		return true;
	}

	// region AD
	if(wADC.p <= 0.0f && wABD.v <= 0.0f && wAD.u > 0.0f && wAD.v > 0.0f)
	{
		simplex.count = 2;
		simplex.vertices[0] = v1;
		simplex.vertices[1] = v4;

		if(wAD.divisor <= 0.0f)
			return false;

		simplex.vertices[0].a = wAD.u / wAD.divisor;
		simplex.vertices[1].a = wAD.v / wAD.divisor;
		return true;
	}

	// region BC
	if(wBCD.p <= 0.0f && wACB.u <= 0.0f && wBC.u > 0.0f && wBC.v > 0.0f)
	{
		simplex.count = 2;
		simplex.vertices[0] = v2;
		simplex.vertices[1] = v3;

		if(wBC.divisor <= 0.0f)
			return false;

		simplex.vertices[0].a = wBC.u / wBC.divisor;
		simplex.vertices[1].a = wBC.v / wBC.divisor;
		return true;
	}

	// region CD
	if(wBCD.u <= 0.0f && wADC.u <= 0.0f && wCD.u > 0.0f && wCD.v > 0.0f)
	{
		simplex.count = 2;
		simplex.vertices[0] = v3;
		simplex.vertices[1] = v4;

		if(wCD.divisor <= 0.0f)
			return false;

		simplex.vertices[0].a = wCD.u / wCD.divisor;
		simplex.vertices[1].a = wCD.v / wCD.divisor;
		return true;
	}

	// region DB
	if(wABD.u <= 0.0f && wBCD.v <= 0.0f && wDB.u > 0.0f && wDB.v > 0.0f)
	{
		simplex.count = 2;
		simplex.vertices[0] = v4;
		simplex.vertices[1] = v2;

		if(wDB.divisor <= 0.0f)
			return false;

		simplex.vertices[0].a = wDB.u / wDB.divisor;
		simplex.vertices[1].a = wDB.v / wDB.divisor;
		return true;
	}

	auto wABCD = tetrahedron_to_barycentric(v1.w, v2.w, v3.w, v4.w);

	// region ACB
	if(wABCD.q < 0.0f && wACB.u > 0.0f && wACB.v > 0.0f && wACB.p > 0.0f)
	{
		simplex.count = 3;
		simplex.vertices[0] = v1;
		simplex.vertices[1] = v3;
		simplex.vertices[2] = v2;

		log::debug("solve4: reduce to ACB");
		if(wACB.divisor <= 0.0f)
			return false;

		simplex.vertices[0].a = wACB.u / wACB.divisor;
		simplex.vertices[1].a = wACB.v / wACB.divisor;
		simplex.vertices[2].a = wACB.p / wACB.divisor;
		return true;
	}

	// region ABD
	if(wABCD.p < 0.0f && wABD.u > 0.0f && wABD.v > 0.0f && wABD.p > 0.0f)
	{
		simplex.count = 3;
		simplex.vertices[0] = v1;
		simplex.vertices[1] = v2;
		simplex.vertices[3] = v4;

		log::debug("solve4: reduce to ABD");
		if(wABD.divisor <= 0.0f)
			return false;

		simplex.vertices[0].a = wABD.u / wABD.divisor;
		simplex.vertices[1].a = wABD.v / wABD.divisor;
		simplex.vertices[2].a = wABD.p / wABD.divisor;
		return true;
	}
	
	// region ADC
	if(wABCD.v < 0.0f && wADC.u > 0.0f && wADC.v > 0.0f && wADC.p > 0.0f)
	{
		simplex.count = 3;
		simplex.vertices[0] = v1;
		simplex.vertices[1] = v4;
		simplex.vertices[2] = v3;

		log::debug("solve4: reduce to ADC");
		if(wADC.divisor <= 0.0f)
			return false;

		simplex.vertices[0].a = wADC.u / wADC.divisor;
		simplex.vertices[1].a = wADC.v / wADC.divisor;
		simplex.vertices[2].a = wADC.p / wADC.divisor;
		return true;
	}

	// region BCD
	if(wABCD.u < 0.0f && wBCD.u > 0.0f && wBCD.v > 0.0f && wBCD.p > 0.0f)
	{
		simplex.count = 3;
		simplex.vertices[0] = v2;
		simplex.vertices[1] = v3;
		simplex.vertices[3] = v4;

		log::debug("solve4: reduce to BCD");
		if(wBCD.divisor <= 0.0f)
			return false;

		simplex.vertices[0].a = wBCD.u / wBCD.divisor;
		simplex.vertices[1].a = wBCD.v / wBCD.divisor;
		simplex.vertices[2].a = wBCD.p / wBCD.divisor;
		return true;
	}
	
	// region ABCD
	if(wABCD.divisor <= 0.0f)
		return false;

	simplex.vertices[0].a = wABCD.u / wABCD.divisor;
	simplex.vertices[1].a = wABCD.v / wABCD.divisor;
	simplex.vertices[2].a = wABCD.p / wABCD.divisor;
	simplex.vertices[3].a = wABCD.q / wABCD.divisor;
	return true;
}

void simplex_compute_witness(const simplex_t& simplex, vec3& out_vA, vec3& out_vB)
{
	assert(simplex.count >= 1 && simplex.count <= 4);

	switch(simplex.count)
	{
	case 1:
		out_vA = simplex.vertices[0].wA;
		out_vB = simplex.vertices[1].wB;
		break;
	case 2:
		log::debug("witness2_A: wA0: {}; wA1: {}", simplex.vertices[0].wA, simplex.vertices[1].wA);
		log::debug("compute_witness2: u: {}; v: {}", simplex.vertices[0].a, simplex.vertices[1].a);
		out_vA = simplex.vertices[0].a * simplex.vertices[0].wA + simplex.vertices[1].a * simplex.vertices[1].wA;
		out_vB = simplex.vertices[0].a * simplex.vertices[0].wB + simplex.vertices[1].a * simplex.vertices[1].wB;
		break;
	case 3:
		log::debug("witness3_A: wA0: {}; wA1: {}; wA2: {}", simplex.vertices[0].wA, simplex.vertices[1].wA, simplex.vertices[2].wA);
		log::debug("compute_witness3: u: {}; v: {}; p: {}", simplex.vertices[0].a, simplex.vertices[1].a, simplex.vertices[2].a);
		out_vA = simplex.vertices[0].a * simplex.vertices[0].wA + simplex.vertices[1].a * simplex.vertices[1].wA + simplex.vertices[2].a * simplex.vertices[2].wA;
		out_vB = simplex.vertices[0].a * simplex.vertices[0].wB + simplex.vertices[1].a * simplex.vertices[1].wB + simplex.vertices[2].a * simplex.vertices[2].wB;
		break;
	case 4:
	{
		const vec3 sum = (simplex.vertices[0].a * simplex.vertices[0].wA + simplex.vertices[1].a * simplex.vertices[1].wA) +
			         (simplex.vertices[2].a * simplex.vertices[2].wA + simplex.vertices[3].a * simplex.vertices[3].wA);
		out_vA = sum;
		out_vB = sum;
		break;
	}
	default:
		std::unreachable();
	}
}

physicsDistanceOutput physics_shape_distance(const physicsDistanceInput& cfg)
{
	log::debug("gjk_start");

	const mat4 transform_2_to_1 = cfg.transform_b.as_matrix() * cfg.transform_a.as_inverse_translation_rotation();
	const mat3 rotT = mat3::transpose(transform_2_to_1.demote<3>());

	simplex_t simplex;
	simplex.count = 0;

	physicsDistanceOutput result;

	if(simplex.count == 0)
	{
		vec3 init_search = vec3::normalize(cfg.transform_b.translation - cfg.transform_a.translation);
		vec3 b_search = -init_search * rotT; 

		simplex.vertices[0].indexA = cfg.shape_a.get_support(init_search);
		simplex.vertices[0].indexB = cfg.shape_b.get_support(b_search);
		simplex.vertices[0].wA = cfg.shape_a.get_point(simplex.vertices[0].indexA);
		vec3 supportB = cfg.shape_b.get_point(simplex.vertices[0].indexB);
		simplex.vertices[0].wB = (vec4{supportB, 1.0f} * transform_2_to_1).demote<3>();
		log::debug("init wA: {}; wB: {}", simplex.vertices[0].wA, simplex.vertices[0].wB);
		simplex.vertices[0].w = simplex.vertices[0].wB - simplex.vertices[0].wA;
		simplex.vertices[0].a = 0.0f;
		simplex.count = 1;
	}
	
	simplex_t backup;
	backup.count = 0;

	float distSq = std::numeric_limits<float>::max();
	constexpr float sd_len_eps = std::numeric_limits<float>::min() * 1000.0f; 
	vec3 normal = vec3{0.0f};

	for(int i = 0; i < 32; i++)
	{
		log::debug("gjk_iter {}", i);
		bool solved = false;
		auto oldc = simplex.count;
		switch(simplex.count)
		{
		case 1:
			simplex.vertices[0].a = 1.0f;
			solved = true;
			break;
		case 2:
			solved = simplex_solve2(simplex);
			break;
		case 3:
			solved = simplex_solve3(simplex);
			break;
		case 4:
			solved = simplex_solve4(simplex);
			break;
		default:
			std::unreachable();
		}
		log::debug("solve{} -> {}-simplex", oldc, simplex.count);

		if(!solved)
		{
			assert(backup.count);
			log::debug("cannot converge");
			simplex = backup;	
			break;
		}

		if(simplex.count == 4)
		{
			log::debug("s4 overlap");
			simplex_compute_witness(simplex, result.point_a, result.point_b);
			return result;
		}

		auto last_distSq = distSq;
		vec3 closest_point = vec3{0.0f};

		switch(simplex.count)
		{
		case 1:
			closest_point = simplex.vertices[0].w;
			break;
		case 2:
			closest_point = simplex.vertices[0].a * simplex.vertices[0].w + simplex.vertices[1].a * simplex.vertices[1].w;
			break;
		case 3:
			closest_point = simplex.vertices[0].a * simplex.vertices[0].w + simplex.vertices[1].a * simplex.vertices[1].w + simplex.vertices[2].a * simplex.vertices[2].w;
			break;
		case 4:
			closest_point = (simplex.vertices[0].a * simplex.vertices[0].w + simplex.vertices[1].a * simplex.vertices[1].w) + 
				        (simplex.vertices[2].a * simplex.vertices[2].w + simplex.vertices[3].a * simplex.vertices[3].w);
			break;
		default:
			std::unreachable();
		}

		distSq = closest_point.magnitude_sqr();
		if(distSq >= last_distSq)
		{
			assert(backup.count);
			log::debug("distance diverging");
			simplex = backup;
			break;
		}

		vec3 search_dir = vec3{0.0f};

		switch(simplex.count)
		{
		case 1:
		{
			search_dir = -simplex.vertices[0].w;
			break;
		}
		case 2:
		{
			const auto a = simplex.vertices[0].w;
			const auto b = simplex.vertices[1].w;
			const auto ab = b - a;
			search_dir = vec3::cross(vec3::cross(ab, -a), ab);
			break;
		}
		case 3:
		{
			const auto a = simplex.vertices[0].w;
			const auto b = simplex.vertices[1].w;
			const auto c = simplex.vertices[2].w;

			const auto ab = b - a;
			const auto ac = c - a;

			const vec3 n = vec3::cross(ab, ac);
			if(n.magnitude_sqr() < 1e-10f)
				log::debug("solve3: degen triangle");
			search_dir = vec3::dot(n, a) < 0.0f ? n : -n;
			break;
		}
		default:
			std::unreachable();
		}

		if(search_dir.magnitude_sqr() < sd_len_eps)
		{
			simplex_compute_witness(simplex, result.point_a, result.point_b);
			return result;
		}

		normal = -search_dir;

		const vec3 search_dirB = search_dir * rotT; 
		const size_t indA = cfg.shape_a.get_support(-search_dir);
		const size_t indB = cfg.shape_b.get_support(search_dirB);
		
		backup = simplex;
		
		bool dup = false;
		for(int i = 0; i < simplex.count; i++)
		{
			if(simplex.vertices[i].indexA == indA && simplex.vertices[i].indexB == indB)
			{
				dup = true;
				break;
			}
		}

		if(dup)
		{
			log::debug("duplicated point, terminating");
			break;
		}

		const vec3 supA = cfg.shape_a.get_point(indA);
		vec3 supB = cfg.shape_b.get_point(indB);
		supB = (vec4(supB, 1.0f) * transform_2_to_1).demote<3>();

		simplex.vertices[simplex.count].indexA = indA;
		simplex.vertices[simplex.count].indexB = indB;
		simplex.vertices[simplex.count].wA = supA;
		simplex.vertices[simplex.count].wB = supB;
		log::debug("simplex_add v{}: wA: {}; wB: {}", simplex.count, supA, supB); 
		simplex.vertices[simplex.count].w = supB - supA;
		simplex.count++;
	}

	normal = vec3::normalize(normal);
	// if not normalized return now

	simplex_compute_witness(simplex, result.point_a, result.point_b);
	log::debug("pA: {}; pB: {} {}-simplex", result.point_a, result.point_b, simplex.count);
	result.distance = (result.point_b - result.point_a).magnitude();
	result.normal = normal;
	return result;
}

physicsShapeCastOutput physics_shape_cast(const physicsShapeCastInput& cfg)
{
	const float linear_slop = 0.005f;
	const float radius_sum = cfg.shape_a.get_convex_radius() + cfg.shape_b.get_convex_radius();
	const float target = std::max(linear_slop, radius_sum - linear_slop);
	float tolerance = 0.25f * linear_slop;

	float alpha = 0.0f;

	physicsDistanceInput dist_cfg
	{
		.shape_a = cfg.shape_a,
		.shape_b = cfg.shape_b,
		.transform_a = cfg.transform_a,
		.transform_b = cfg.transform_b
	};
	physicsDistanceOutput dist_res;
	physicsShapeCastOutput result;
	result.hit = false;
	result.fraction = 1.0f;

	vec3 delta2 = cfg.direction;

	for(int i = 0; i < 20; i++)
	{
		dist_res = physics_shape_distance(dist_cfg);
		log::debug("gjk_cast: iter {} dist {}", i, dist_res.distance);
		if(dist_res.distance < target + tolerance)
		{
			if(i == 0)
			{
				result.hit = true;
			
				vec3 c1 = dist_res.point_a + cfg.shape_a.get_convex_radius() * dist_res.normal;
				vec3 c2 = dist_res.point_b - cfg.shape_b.get_convex_radius() * dist_res.normal;
				result.point = mix(c1, c2, 0.5f);
				return result;
			}
			else
			{
				result.fraction = alpha;
				result.point = dist_res.point_a + cfg.shape_a.get_convex_radius() * dist_res.normal;
				result.normal = dist_res.normal;
				result.hit = true;
				return result;
			}
		}

		float denom = vec3::dot(delta2, dist_res.normal);
		if(denom >= 0.0f)
			return result;

		alpha += (target - dist_res.distance) / denom;
		if(alpha >= cfg.fraction)
			return result;

		dist_cfg.transform_b.translation += alpha * delta2;
	}

	return result;
}

}


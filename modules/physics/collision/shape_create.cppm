module penumbra.physics:shape_create;

import :api;
import :shape;

import penumbra.core;
import penumbra.math;

import std;
using std::size_t, std::uint8_t;

namespace penumbra
{

RefCounted<physicsShape> physics_create_sphere(const physicsSphereDesc& desc)
{
        auto* col = new physicsSphereShape();
        col->bounds.mins = vec3{-desc.radius};
        col->bounds.maxs = vec3{desc.radius};
        col->radius = desc.radius;
        col->density = desc.density;

        const float r2 = desc.radius * desc.radius;
        const float volume = (4.0f / 3.0f) * r2 * desc.radius * std::numbers::pi_v<float>;
        const float mass = volume * desc.density;
        col->mass = mass;

        float inertia = (2.0f / 5.0f) * mass * r2;
        col->inertia_tensor = mat3
        {
                vec3::basis(0) * inertia,
                vec3::basis(1) * inertia,
                vec3::basis(2) * inertia
        };
        return RefCounted<physicsShape>{static_cast<physicsShape*>(col)};
}

RefCounted<physicsShape> physics_create_capsule(const physicsCapsuleDesc& desc)
{
        auto* col = new physicsCapsuleShape();
        col->bounds.mins = vec3{-desc.radius, -desc.radius - (desc.height * 0.5f), -desc.radius};
        col->bounds.maxs = vec3{desc.radius, desc.radius + (desc.height * 0.5f), desc.radius};
        col->radius = desc.radius;
        col->height = desc.height;
        col->density = desc.density;

        const float r2 = desc.radius * desc.radius;
        const float cm = desc.density * desc.height * r2 * std::numbers::pi_v<float>;
        const float hm = (2.0f / 3.0f) * std::numbers::pi_v<float> * r2 * desc.radius * desc.density;
        col->mass = cm + (2.0f * hm);

        const float h2 = desc.height * desc.height;
        const float inertia_y = (cm * r2 * 0.5f) + ((2.0f * hm) * (2.0f * r2 / 5.0f));
        const float inertia_xz = (cm * (h2 / 12.0f + r2 / 4.0f)) + ((2.0f * hm) * ((2.0f * r2 / 5.0f) + (h2 / 4.0f) + (3.0f * desc.height * desc.radius / 8.0f)));

        col->inertia_tensor = mat3
        {
                vec3::basis(0) * inertia_xz,
                vec3::basis(1) * inertia_y,
                vec3::basis(2) * inertia_xz
        };

        return RefCounted<physicsShape>{static_cast<physicsShape*>(col)};
}

RefCounted<physicsShape> physics_create_box(const physicsBoxDesc& desc)
{
        const float xe = desc.edges.x * 0.5f;
        const float ye = desc.edges.y * 0.5f;
        const float ze = desc.edges.z * 0.5f;

        auto* col = new physicsHullShape();
        col->bounds.mins = vec3{-xe, -ye, -ze};
        col->bounds.maxs = vec3{xe, ye, ze};
        col->density = desc.density;
        const float volume = desc.edges.x * desc.edges.y * desc.edges.z;
        const float mass = volume * desc.density;
        col->mass = mass;

        col->vertices =
        {
                vec3{xe, ye, ze},
                vec3{-xe, ye, ze},
                vec3{-xe, -ye, ze},
                vec3{xe, -ye, ze},
                vec3{xe, ye, -ze},
                vec3{-xe, ye, -ze},
                vec3{-xe, -ye, -ze},
                vec3{xe, -ye, -ze}
        };

        col->planes =
        {
                Plane(-1.0f, 0.0f, 0.0f, xe),
                Plane(1.0f, 0.0f, 0.0f, xe),
                Plane(0.0f, -1.0f, 0.0f, ye),
                Plane(0.0f, 1.0f, 0.0f, ye),
                Plane(0.0f, 0.0f, -1.0f, ze),
                Plane(0.0f, 0.0f, 1.0f, ze),
        };

        col->plane_to_edge =
        {
                0,
                8,
                16,
                20,
                19,
                21
        };

	col->edges =
        {
                hull_halfedge{2, 1, 2, 0}, hull_halfedge{17, 0, 1, 5},
                hull_halfedge{4, 3, 1, 0}, hull_halfedge{20, 2, 5, 3},
                hull_halfedge{6, 5, 5, 0}, hull_halfedge{23, 4, 6, 4},
                hull_halfedge{0, 7, 6, 0}, hull_halfedge{18, 6, 2, 2},
                hull_halfedge{10, 9, 0, 1}, hull_halfedge{21, 8, 3, 5},
                hull_halfedge{12, 11, 3, 1}, hull_halfedge{16, 10, 7, 2},
                hull_halfedge{14, 13, 7, 1}, hull_halfedge{19, 12, 4, 4},
                hull_halfedge{8, 15, 4, 1}, hull_halfedge{22, 14, 0, 3},
                hull_halfedge{7, 17, 3, 2}, hull_halfedge{9, 16, 2, 5},
                hull_halfedge{11, 19, 6, 2}, hull_halfedge{5, 18, 7, 4},
                hull_halfedge{15, 21, 1, 3}, hull_halfedge{1, 20, 0, 5},
                hull_halfedge{3, 23, 4, 3}, hull_halfedge{13, 22, 5, 4}
        };

        const float ex2 = desc.edges.x * desc.edges.x;
        const float ey2 = desc.edges.y * desc.edges.y;
        const float ez2 = desc.edges.z * desc.edges.z;

        const float x = mass / 12.0f * (ey2 + ez2);
        const float y = mass / 12.0f * (ex2 + ez2);
        const float z = mass / 12.0f * (ex2 + ey2);

        col->inertia_tensor = mat3
        {
                vec3::basis(0) * x,
                vec3::basis(1) * y,
                vec3::basis(2) * z
        };

	return RefCounted<physicsShape>{static_cast<physicsShape*>(col)};
}

}

export module penumbra.editor:camera_component;

import std;

namespace penumbra
{

export enum CameraProjection : int
{
	CAMERA_PROJECTION_PERSPECTIVE,
	CAMERA_PROJECTION_ORTHOGRAPHIC
};

export struct camera_component
{
	float vertical_fov{70.0f};
	float near_plane{0.1f};
	float far_plane{128.0f};
	CameraProjection projection{CAMERA_PROJECTION_PERSPECTIVE};

	float aperture{8.0f};
	float shutter_speed{60.0f};
	float iso{100.0f};
	float exposure_compensation{0.0f};

	float get_exposure() const
	{
		float ev100 = std::log2f(((aperture * aperture) / (1.0f / shutter_speed)) * (100 / iso)) - exposure_compensation;
		return 1.0f / (1.2f * std::powf(2.0f, ev100));
	}
};

}

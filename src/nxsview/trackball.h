#ifndef NX_TRACKBALL_H
#define NX_TRACKBALL_H

#include <algorithm>
#include <cmath>

#include "../core/mesh_types.h"

// Turntable trackball: yaw around the vertical axis, pitch around the screen horizontal axis,
// uniform zoom and object space pan. Transform = scale * rotation * translate(pan).
class Trackball {
public:
	enum Button { BUTTON_NONE = 0, BUTTON_LEFT = 1, BUTTON_MIDDLE = 2, BUTTON_RIGHT = 4, KEY_CTRL = 8 };

	struct Pose {
		float yaw = 0;
		float pitch = 0;
		float scale = 1;
		nx::Vector3f pan{0, 0, 0};

		Pose operator*(float w) const { return {yaw*w, pitch*w, scale*w, pan*w}; }
		Pose &operator+=(const Pose &p) {
			yaw += p.yaw; pitch += p.pitch; scale += p.scale; pan += p.pan;
			return *this;
		}
	};

	Pose pose;
	float pixel_size = 0.005f; // world units (at the center depth) per pixel

	nx::Matrix4f matrix() const {
		return nx::Matrix4f::scale(pose.scale) * rotation() * nx::Matrix4f::translation(pose.pan);
	}

	void reset() { pose = Pose(); }

	// Moves the object so that p (expressed after the trackball transform) ends up at the center.
	void translate(const nx::Vector3f &p) {
		pose.pan += inverseRotation() * (-p) / pose.scale;
	}
	void scale(float s) { pose.scale *= s; }

	void wheel(int direction) { scale(direction > 0 ? 1.1f : 1/1.1f); }

	void mouseDown(int x, int y, int buttons) {
		last_x = x; last_y = y;
		this->buttons = buttons;
	}

	void mouseMove(int x, int y) {
		int dx = x - last_x, dy = y - last_y;
		last_x = x; last_y = y;

		if(buttons & (BUTTON_MIDDLE | BUTTON_RIGHT)  || ((buttons & BUTTON_LEFT) && (buttons & KEY_CTRL))) {
			nx::Vector3f d{dx * pixel_size, dy * pixel_size, 0};
			pose.pan += inverseRotation() * d / pose.scale;
		} else if(buttons & BUTTON_LEFT) {
			const float half_pi = 1.5707963f;
			pose.yaw += dx * 0.005f;
			pose.pitch = std::clamp(pose.pitch - dy * 0.005f, -half_pi, half_pi);
		}
	}

	void mouseUp(int, int, int) { buttons = BUTTON_NONE; }

private:
	int last_x = 0, last_y = 0;
	int buttons = BUTTON_NONE;

	nx::Matrix4f rotation() const {
		return nx::Matrix4f::rotationX(pose.pitch) * nx::Matrix4f::rotationY(pose.yaw);
	}
	nx::Matrix4f inverseRotation() const {
		return nx::Matrix4f::rotationY(-pose.yaw) * nx::Matrix4f::rotationX(-pose.pitch);
	}
};

#endif // NX_TRACKBALL_H

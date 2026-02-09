#include "texture.h"

#include <algorithm>
#include <cstring>
#include <iostream>

namespace nx {

/*
Vector3f Texture::sample(float x, float y, int level) {
	if (image_data_.empty() || width_ <= 0 || height_ <= 0) {
		return {0.0f, 0.0f, 0.0f};
	}
	if (level < 0) {
		level = 0;
	}
	ensure_mip_level(level);
	if (level >= static_cast<int>(mip_levels_.size())) {
		level = static_cast<int>(mip_levels_.size()) - 1;
	}
	const std::vector<uint8_t>& data = mip_levels_[level];
	int w = mip_sizes_[level].first;
	int h = mip_sizes_[level].second;
	if (w <= 0 || h <= 0) {
		return {0.0f, 0.0f, 0.0f};
	}

	float u = std::clamp(x, 0.0f, 1.0f) * (w - 1);
	float v = std::clamp(y, 0.0f, 1.0f) * (h - 1);

	int x0 = static_cast<int>(std::floor(u));
	int y0 = static_cast<int>(std::floor(v));
	int x1 = std::min(x0 + 1, w - 1);
	int y1 = std::min(y0 + 1, h - 1);

	float tx = u - static_cast<float>(x0);
	float ty = v - static_cast<float>(y0);

	auto fetch = [&](int px, int py) {
		const uint8_t* src = &data[(py * w + px) * 3];
		return Vector3f{src[0] / 255.0f, src[1] / 255.0f, src[2] / 255.0f};
	};

	Vector3f c00 = fetch(x0, y0);
	Vector3f c10 = fetch(x1, y0);
	Vector3f c01 = fetch(x0, y1);
	Vector3f c11 = fetch(x1, y1);

	Vector3f c0{
		c00.x + (c10.x - c00.x) * tx,
		c00.y + (c10.y - c00.y) * tx,
		c00.z + (c10.z - c00.z) * tx
	};
	Vector3f c1{
		c01.x + (c11.x - c01.x) * tx,
		c01.y + (c11.y - c01.y) * tx,
		c01.z + (c11.z - c01.z) * tx
	};

	return {
		c0.x + (c1.x - c0.x) * ty,
		c0.y + (c1.y - c0.y) * ty,
		c0.z + (c1.z - c0.z) * ty
	};
}
 */
} // namespace nx

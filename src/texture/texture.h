#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <QString>

#include "../core/mapped_file.h"
#include "../core/mesh_types.h"

namespace nx {

class Texture {
public:
	bool load(const QString& filename);
	bool build_pyramid(const std::string& output_path, uint32_t tile_size = 256, uint32_t overlap = 1);
	Vector3f sample(float x, float y, int level);

	int width() const { return width_; }
	int height() const { return height_; }

private:
	std::vector<uint8_t> image_data_;
	int width_ = 0;
	int height_ = 0;
	std::vector<std::vector<uint8_t>> mip_levels_;
	std::vector<std::pair<int, int>> mip_sizes_;

	static std::vector<uint8_t> downsample_gaussian(const std::vector<uint8_t>& src, int width, int height);
	void ensure_mip_level(int level);
	static void write_level_tiles(uint8_t* base,
		const std::vector<uint8_t>& level,
		int width,
		int height,
		uint32_t tile_size,
		uint32_t overlap);
};

} // namespace nx

#include "pushpull.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <utility>

namespace nx {

void pushPullFillUnwrittenPixels(int width,
	int height,
	std::vector<uint8_t>& texels,
	const std::vector<uint8_t>& mask,
	const std::vector<Material>& materials) {
	(void)materials;
	assert(width > 0 && height > 0);

	const size_t pixel_count = static_cast<size_t>(width) * static_cast<size_t>(height);
	assert(mask.size() == pixel_count);
	assert(texels.size() % (pixel_count * 3) == 0);
	const size_t slot_count = texels.size() / (pixel_count * 3);
	assert(slot_count > 0);

	bool has_holes = false;
	bool has_any_valid = false;
	for(uint8_t value: mask) {
		has_holes = has_holes || (value == 0);
		has_any_valid = has_any_valid || (value != 0);
	}
	if(!has_holes || !has_any_valid)
		return;

	struct Level {
		int width = 0;
		int height = 0;
		std::vector<uint8_t> mask;
		std::vector<uint8_t> texels;
	};

	std::vector<Level> levels;
	levels.reserve(16);
	levels.push_back({width, height, mask, texels});

	while(levels.back().width > 1 || levels.back().height > 1) {
		const Level& fine = levels.back();
		Level coarse;
		coarse.width = std::max(1, fine.width / 2);
		coarse.height = std::max(1, fine.height / 2);
		const size_t coarse_pixels = static_cast<size_t>(coarse.width) * static_cast<size_t>(coarse.height);
		coarse.mask.assign(coarse_pixels, 0);
		coarse.texels.assign(coarse_pixels * slot_count * 3, 0);

		const size_t fine_stride = static_cast<size_t>(fine.width) * static_cast<size_t>(fine.height) * 3;
		const size_t coarse_stride = coarse_pixels * 3;

		for(int cy = 0; cy < coarse.height; ++cy) {
			for(int cx = 0; cx < coarse.width; ++cx) {
				const int fx0 = cx * 2;
				const int fy0 = cy * 2;
				const size_t cidx = static_cast<size_t>(cy) * static_cast<size_t>(coarse.width) + static_cast<size_t>(cx);

				for(size_t slot = 0; slot < slot_count; ++slot) {
					int sum0 = 0;
					int sum1 = 0;
					int sum2 = 0;
					int valid_count = 0;

					for(int oy = 0; oy < 2; ++oy) {
						const int fy = fy0 + oy;
						if(fy >= fine.height)
							continue;
						for(int ox = 0; ox < 2; ++ox) {
							const int fx = fx0 + ox;
							if(fx >= fine.width)
								continue;

							const size_t fidx = static_cast<size_t>(fy) * static_cast<size_t>(fine.width) + static_cast<size_t>(fx);
							if(!fine.mask[fidx])
								continue;

							const size_t src = slot * fine_stride + fidx * 3;
							sum0 += static_cast<int>(fine.texels[src + 0]);
							sum1 += static_cast<int>(fine.texels[src + 1]);
							sum2 += static_cast<int>(fine.texels[src + 2]);
							valid_count++;
						}
					}

					if(valid_count == 0)
						continue;

					coarse.mask[cidx] = 1;
					const size_t dst = slot * coarse_stride + cidx * 3;
					coarse.texels[dst + 0] = static_cast<uint8_t>(sum0 / valid_count);
					coarse.texels[dst + 1] = static_cast<uint8_t>(sum1 / valid_count);
					coarse.texels[dst + 2] = static_cast<uint8_t>(sum2 / valid_count);
				}
			}
		}

		levels.push_back(std::move(coarse));
	}

	for(int level = static_cast<int>(levels.size()) - 1; level > 0; --level) {
		const Level& coarse = levels[static_cast<size_t>(level)];
		Level& fine = levels[static_cast<size_t>(level - 1)];
		const size_t fine_stride = static_cast<size_t>(fine.width) * static_cast<size_t>(fine.height) * 3;
		const size_t coarse_stride = static_cast<size_t>(coarse.width) * static_cast<size_t>(coarse.height) * 3;

		for(int fy = 0; fy < fine.height; ++fy) {
			for(int fx = 0; fx < fine.width; ++fx) {
				const size_t fidx = static_cast<size_t>(fy) * static_cast<size_t>(fine.width) + static_cast<size_t>(fx);
				if(fine.mask[fidx])
					continue;

				const float gx = (static_cast<float>(fx) + 0.5f) * 0.5f - 0.5f;
				const float gy = (static_cast<float>(fy) + 0.5f) * 0.5f - 0.5f;
				const int x0 = std::clamp(static_cast<int>(std::floor(gx)), 0, coarse.width - 1);
				const int y0 = std::clamp(static_cast<int>(std::floor(gy)), 0, coarse.height - 1);
				const int x1 = std::min(x0 + 1, coarse.width - 1);
				const int y1 = std::min(y0 + 1, coarse.height - 1);
				const float tx = std::clamp(gx - static_cast<float>(x0), 0.0f, 1.0f);
				const float ty = std::clamp(gy - static_cast<float>(y0), 0.0f, 1.0f);

				const int sample_x[4] = {x0, x1, x0, x1};
				const int sample_y[4] = {y0, y0, y1, y1};
				const float sample_w[4] = {
					(1.0f - tx) * (1.0f - ty),
					tx * (1.0f - ty),
					(1.0f - tx) * ty,
					tx * ty
				};

				for(size_t slot = 0; slot < slot_count; ++slot) {
					const size_t dst = slot * fine_stride + fidx * 3;

					for(int channel = 0; channel < 3; ++channel) {
						float weighted_sum = 0.0f;
						float weight_sum = 0.0f;
						for(int s = 0; s < 4; ++s) {
							const size_t cidx = static_cast<size_t>(sample_y[s]) * static_cast<size_t>(coarse.width) + static_cast<size_t>(sample_x[s]);
							if(!coarse.mask[cidx])
								continue;
							const float w = sample_w[s];
							const size_t src = slot * coarse_stride + cidx * 3;
							weighted_sum += w * static_cast<float>(coarse.texels[src + static_cast<size_t>(channel)]);
							weight_sum += w;
						}
						if(weight_sum > 0.0f) {
							fine.texels[dst + static_cast<size_t>(channel)] = static_cast<uint8_t>(std::lround(weighted_sum / weight_sum));
						}
					}
				}

				float coarse_weight_sum = 0.0f;
				for(int s = 0; s < 4; ++s) {
					const size_t cidx = static_cast<size_t>(sample_y[s]) * static_cast<size_t>(coarse.width) + static_cast<size_t>(sample_x[s]);
					if(coarse.mask[cidx])
						coarse_weight_sum += sample_w[s];
				}
				if(coarse_weight_sum > 0.0f)
					fine.mask[fidx] = 1;
			}
		}
	}

	texels.swap(levels[0].texels);
}

}

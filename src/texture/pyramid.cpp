#include "pyramid.h"
#include "jpeg_decoder.h"

#include <QFileInfo>

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <random>
#include <algorithm>

#include <tiffio.h>

#include <assert.h>

using namespace std;


namespace {

constexpr int kGaussianKernel[5] = {1, 4, 6, 4, 1};
constexpr int kGaussianNorm = 16;

std::vector<uint8_t> downsampleGaussian(const std::vector<uint8_t> &src, int width, int height, int nc) {
	if(width < 2 || height < 2)
		return {};

	int halfW = width >> 1;
	int halfH = height >> 1;
	std::vector<uint8_t> horiz(halfW * height * 3);
	for(int y = 0; y < height; ++y) {
		for(int x = 0; x < halfW; ++x) {
			int center = 2*x + 1;
			for(int c = 0; c < nc; ++c) {
				int acc = 0;
				for(int k = -2; k <= 2; ++k) {
					int ix = std::min(std::max(center + k, 0), width - 1);
					acc += kGaussianKernel[k + 2] * src[(y*width + ix)*nc + c];
				}
				horiz[(y*halfW + x)*nc + c] = static_cast<uint8_t>((acc + (kGaussianNorm/2)) / kGaussianNorm);
			}
		}
	}
	std::vector<uint8_t> out(halfW * halfH * nc);
	for(int y = 0; y < halfH; ++y) {
		int centerY = 2*y + 1;
		for(int x = 0; x < halfW; ++x) {
			for(int c = 0; c < nc; ++c) {
				int acc = 0;
				for(int k = -2; k <= 2; ++k) {
					int iy = std::min(std::max(centerY + k, 0), height - 1);
					acc += kGaussianKernel[k + 2] * horiz[(iy*halfW + x)*nc + c];
				}
				out[(y*halfW + x)*nc + c] = static_cast<uint8_t>((acc + (kGaussianNorm/2)) / kGaussianNorm);
			}
		}
	}
	return out;
}
}

namespace nx {

TileRow::TileRow(int _tileside, int _ncomponents, PyrLevel &_level) {
	tileside = _tileside;
	ncomponents = _ncomponents;
	level = _level;
	end_tile = 0;
	downsampleWidth = level.width > 1 ? level.width >> 1 : 0;
	scaledLinesTarget = level.height > 1 ? level.height >> 1 : 0;
	hasNextLevel = downsampleWidth > 0 && scaledLinesTarget > 0;
	nextRow();
}

void TileRow::writeLine(const std::vector<uint8_t> &newline) {
	if(newline.empty())
		return;

	int tile_row = current_row;
	int line_in_tile = current_line - tile_row * tileside;

	int tile_size = tileside * tileside * ncomponents;
	int line_stride = tileside * ncomponents;

	for(int col = 0; col < level.cols; ++col) {
		int src_x = col * tileside;
		int copy_w = std::min(tileside, level.width - src_x);

		uint8_t *tile_start = level.start + (tile_row * level.cols + col) * tile_size;
		uint8_t *dst = tile_start + line_in_tile * line_stride;
		const uint8_t *src = newline.data() + src_x * ncomponents;
		std::memcpy(dst, src, static_cast<size_t>(copy_w) * ncomponents);
	}
}

std::vector<uint8_t> TileRow::addLine(const std::vector<uint8_t> &newline) {
	if(newline.empty())
		return {};

	writeLine(newline);
	lastInputLine = current_line;

	if(hasNextLevel) {
		std::vector<uint8_t> filteredLine = applyHorizontalFilter(newline);
		filtered.push_back({lastInputLine, std::move(filteredLine)});
	}

	current_line++;

	if(current_line > end_tile)
		overlapping.push_back(newline);

	if(current_line == end_tile) {
		clear();
	}

	if(current_line == end_tile && current_line != level.height) {
		nextRow();
	}

	return emitReadyScaledLine();
}

void TileRow::finalizeInput() {
	inputCompleted = true;
}

std::vector<uint8_t> TileRow::drainScaledLine() {
	return emitReadyScaledLine();
}

std::vector<uint8_t> TileRow::emitReadyScaledLine() {
	if(!hasNextLevel)
		return {};
	if(scaledLinesProduced >= scaledLinesTarget)
		return {};
	int center = 1 + scaledLinesProduced * 2;
	if(!inputCompleted && lastInputLine < center + 2)
		return {};
	if(filtered.empty())
		return {};

	std::array<const std::vector<uint8_t>*, 5> lines;
	for(int k = -2; k <= 2; ++k) {
		int idx = std::min(std::max(center + k, 0), level.height - 1);
		lines[k + 2] = &lineForIndex(idx);
	}
	std::vector<uint8_t> scaled = applyVerticalFilter(lines);
	scaledLinesProduced++;
	expireObsoleteLines(center);
	return scaled;
}

std::vector<uint8_t> TileRow::applyHorizontalFilter(const std::vector<uint8_t> &line) const {
	if(!hasNextLevel)
		return {};
	std::vector<uint8_t> filtered(downsampleWidth * 3);
	for(int i = 0; i < downsampleWidth; ++i) {
		int center = 2*i + 1;
		for(int c = 0; c < 3; ++c) {
			int acc = 0;
			for(int k = -2; k <= 2; ++k) {
				int src = std::min(std::max(center + k, 0), level.width - 1);
				acc += kGaussianKernel[k + 2] * line[src*3 + c];
			}
			filtered[i*3 + c] = static_cast<uint8_t>((acc + (kGaussianNorm/2)) / kGaussianNorm);
		}
	}
	return filtered;
}

std::vector<uint8_t> TileRow::applyVerticalFilter(const std::array<const std::vector<uint8_t>*,5> &lines) const {
	std::vector<uint8_t> scaled(downsampleWidth * 3);
	for(int x = 0; x < downsampleWidth; ++x) {
		for(int c = 0; c < 3; ++c) {
			int acc = 0;
			for(int k = 0; k < 5; ++k) {
				acc += kGaussianKernel[k] * (*(lines[k]))[x*3 + c];
			}
			scaled[x*3 + c] = static_cast<uint8_t>((acc + (kGaussianNorm/2)) / kGaussianNorm);
		}
	}
	return scaled;
}

const std::vector<uint8_t> &TileRow::lineForIndex(int index) const {
	assert(!filtered.empty());
	if(index <= filtered.front().index)
		return filtered.front().data;
	if(index >= filtered.back().index)
		return filtered.back().data;
	for(const auto &entry : filtered) {
		if(entry.index == index)
			return entry.data;
	}
	return filtered.back().data;
}

void TileRow::expireObsoleteLines(int centerIndex) {
	int minIndex = centerIndex - 2;
	while(!filtered.empty() && filtered.front().index < minIndex) {
		filtered.pop_front();
	}
}

void TileRow::nextRow() {
	current_row++;

	int start_tile = current_row*tileside;
	end_tile = std::min(level.height, (current_row+1)*tileside);
	int h = end_tile - start_tile;

	for(auto &line: overlapping) {
		writeLine(line);
	}
	overlapping.clear();
}

bool Pyramid::build(const std::string &input, const std::string &cache_dir, int tile_size) {
	tileside = tile_size;

	return buildTiledImages(input, cache_dir);
}

bool Pyramid::buildTiledImages(const std::string &input, const string &cache_dir) {
	std::filesystem::path cache_root(cache_dir);
	std::filesystem::path input_path(input);
	std::string stem = input_path.stem().string();
	std::random_device rd;
	std::mt19937 rng(rd());
	std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFFu);
	cache_path = (cache_root / (stem + "_" + std::to_string(dist(rng)) + ".pyr")).string();

	
	JpegDecoder decoder;
	if(!decoder.init(input.c_str(), width, height))
		throw std::runtime_error("Could not read image file" + input);

	std::vector<TileRow> rows = initRows();

	std::vector<uint8_t> line(width * 3);
	std::vector<uint8_t> carry;

	for(int y = 0; y < height; ++y) {
		decoder.readRows(1, line.data());
		const std::vector<uint8_t> *current = &line;
		carry.clear();
		for(size_t level = 0; level < rows.size(); ++level) {
			std::vector<uint8_t> next = rows[level].addLine(*current);
			if(next.empty())
				break;
			carry = std::move(next);
			current = &carry;
		}
	}

	flushLevels(rows);
	exportPyramid(cache_dir);

	return true;
}

int Pyramid::nLevels() {
	int level = 1;
	int w = width;
	int h = height;
	while(w > tileside || h > tileside) {
		w = std::max(1, w >> 1);
		h = std::max(1, h >> 1);
		level++;
	}
	return level;
}

std::vector<TileRow>  Pyramid::initRows() {
	std::vector<TileRow> rows;

	int n_levels = nLevels();
	levels.resize(n_levels);


	int tot_tiles = 0;
	int w = width;
	int h = height;
	for(int level = n_levels - 1; level >= 0; --level) {
		PyrLevel &pyr_level = levels[level];
		pyr_level.width = w;
		pyr_level.height = h;
		pyr_level.rows = std::max(1, (w + tileside - 1) / tileside);
		pyr_level.cols = std::max(1, (h + tileside - 1) / tileside);
		tot_tiles += pyr_level.rows*pyr_level.cols;
		w = std::max(1, w >> 1);
		h = std::max(1, h >> 1);
	}

	std::size_t total_bytes = static_cast<std::size_t>(tot_tiles) * tileside * tileside * ncomponents;
	if (!data.open(cache_path, MappedFile::READ_WRITE, total_bytes)) {
		throw std::runtime_error("Could not create pyramid cache file: " + cache_path);
	}
	uint8_t *start = data.data();
	w = width;
	h = height;
	for(int level = n_levels - 1; level >= 0; --level) {
		PyrLevel &pyr_level = levels[level];
		pyr_level.start = start;
		start += pyr_level.rows*pyr_level.cols*tileside*tileside*ncomponents;
		rows.emplace_back(tileside, ncomponents, pyr_level);
		w = std::max(1, w >> 1);
		h = std::max(1, h >> 1);
	}
	return rows;
}

void Pyramid::flushLevels(std::vector<TileRow> &rows) {
	for(size_t level = 0; level < rows.size(); ++level) {
		rows[level].finalizeInput();
		while(true) {
			std::vector<uint8_t> pending = rows[level].drainScaledLine();
			if(pending.empty())
				break;
			const std::vector<uint8_t> *current = &pending;
			for(size_t next = level + 1; next < rows.size(); ++next) {
				std::vector<uint8_t> forwarded = rows[next].addLine(*current);
				if(forwarded.empty()) {
					current = nullptr;
					break;
				}
				pending = std::move(forwarded);
				current = &pending;
			}
		}
	}
}

void Pyramid::exportPyramid(const std::string &output_dir) const {
	if (levels.empty() || !data.data())
		return;

	std::filesystem::path out_dir(output_dir);
	std::filesystem::create_directories(out_dir);

	const int tile_size = tileside;
	const int bytes_per_pixel = ncomponents;
	const int tile_bytes = tile_size * tile_size * bytes_per_pixel;

	for (std::size_t level_index = 0; level_index < levels.size(); ++level_index) {
		const PyrLevel& level = levels[level_index];
		if (!level.start || level.width <= 0 || level.height <= 0)
			continue;

		std::filesystem::path out_path = out_dir / ("pyramid_level_" + std::to_string(level_index) + ".ppm");
		std::ofstream file(out_path, std::ios::binary);
		if (!file.is_open())
			continue;

		file << "P6\n" << level.width << " " << level.height << "\n255\n";

		std::vector<uint8_t> scanline(static_cast<std::size_t>(level.width) * 3, 0);
		for (int y = 0; y < level.height; ++y) {
			int tile_row = y / tile_size;
			int in_tile_y = y - tile_row * tile_size;
			for (int x = 0; x < level.width; ++x) {
				int tile_col = x / tile_size;
				int in_tile_x = x - tile_col * tile_size;
				std::size_t tile_index = static_cast<std::size_t>(tile_row) * level.cols + tile_col;
				const uint8_t* tile = level.start + tile_index * tile_bytes;
				const uint8_t* src = tile + (in_tile_y * tile_size + in_tile_x) * bytes_per_pixel;
				std::size_t dst_idx = static_cast<std::size_t>(x) * 3;
				scanline[dst_idx + 0] = src[0];
				scanline[dst_idx + 1] = (bytes_per_pixel > 1) ? src[1] : src[0];
				scanline[dst_idx + 2] = (bytes_per_pixel > 2) ? src[2] : src[0];
			}
			file.write(reinterpret_cast<const char*>(scanline.data()), static_cast<std::streamsize>(scanline.size()));
		}
	}
}

Vector3f Pyramid::sample(float u, float v, int level) const {
	if (levels.empty() || !data.data()) {
		return {0.0f, 0.0f, 0.0f};
	}
	if (level < 0) level = 0;
	if (level >= static_cast<int>(levels.size()))
		level = static_cast<int>(levels.size()) - 1;
	const PyrLevel& lvl = levels[level];
	if (lvl.width <= 0 || lvl.height <= 0) {
		return {0.0f, 0.0f, 0.0f};
	}
	const float clamp_u = std::clamp(u, 0.0f, 1.0f);
	const float clamp_v = std::clamp(v, 0.0f, 1.0f);
	const float fx = clamp_u * (lvl.width - 1);
	const float fy = clamp_v * (lvl.height - 1);
	int x0 = static_cast<int>(std::floor(fx));
	int y0 = static_cast<int>(std::floor(fy));
	int x1 = std::min(x0 + 1, lvl.width - 1);
	int y1 = std::min(y0 + 1, lvl.height - 1);
	float tx = fx - static_cast<float>(x0);
	float ty = fy - static_cast<float>(y0);

	auto fetch = [&](int px, int py) {
		int tile_col = px / tileside;
		int tile_row = py / tileside;
		int in_tile_x = px - tile_col * tileside;
		int in_tile_y = py - tile_row * tileside;
		if (tile_col < 0 || tile_row < 0 || tile_col >= lvl.cols || tile_row >= lvl.rows) {
			return Vector3f{0.0f, 0.0f, 0.0f};
		}
		const int tile_bytes = tileside * tileside * ncomponents;
		const std::size_t tile_index = static_cast<std::size_t>(tile_row) * lvl.cols + tile_col;
		const uint8_t* tile = lvl.start + tile_index * tile_bytes;
		const uint8_t* src = tile + (in_tile_y * tileside + in_tile_x) * ncomponents;
		float r = src[0] / 255.0f;
		float g = (ncomponents > 1) ? src[1] / 255.0f : r;
		float b = (ncomponents > 2) ? src[2] / 255.0f : r;
		return Vector3f{r, g, b};
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

}

#ifndef PYRAMID_H
#define PYRAMID_H

#include <string>
#include <vector>
#include <deque>
#include <array>

#include "../core/mapped_file.h"
#include <QString>

namespace nx {

class Tile {
public:
	int width;
	uint8_t *line = nullptr;
};

struct PyrLevel {
	uint8_t *start = nullptr;
	int width = 0, height = 0;
	int cols = 0, rows = 0;
};

class TileRow: public std::vector<Tile> {
public:
	int tileside;
	int ncomponents;
	PyrLevel level;

	int current_row = -1;
	int current_line = 0;                          //keeps track of which image line we are processing
	int end_tile = 0;                              //wich line ends the current tile
	std::vector<uint8_t> lastLine;                 //keep previous line for scaling
	std::vector<std::vector<uint8_t>> overlapping; //overlapped regions is kept, to be inserted in the new row


	TileRow() {}
	TileRow(int _tileside, int ncomponents, PyrLevel &level);
	void nextRow();
	void finishRow();

	//returns resized line once the gaussian kernel can be applied, or empty array
	std::vector<uint8_t> addLine(const std::vector<uint8_t> &line);
	void finalizeInput();
	std::vector<uint8_t> drainScaledLine();

private:
	//actually write the line to the jpegs
	void writeLine(const std::vector<uint8_t> &newline);

	std::vector<uint8_t> emitReadyScaledLine();
	std::vector<uint8_t> applyHorizontalFilter(const std::vector<uint8_t> &line) const;
	std::vector<uint8_t> applyVerticalFilter(const std::array<const std::vector<uint8_t>*,5> &lines) const;
	const std::vector<uint8_t> &lineForIndex(int index) const;
	void expireObsoleteLines(int centerIndex);

	struct FilteredLine {
		int index = 0;
		std::vector<uint8_t> data;
	};
	std::deque<FilteredLine> filtered;
	int lastInputLine = -1;
	int scaledLinesProduced = 0;
	int scaledLinesTarget = 0;
	int downsampleWidth = 0;
	bool hasNextLevel = false;
	bool inputCompleted = false;
};


class Pyramid {
public:
	int ncomponents = 3;
	int tileside = 256;
	int width, height;
	int quality; //0 100 jpeg quality

	bool build(const std::string &filename, const std::string &cache_dir, int tile_size = 254);
	void exportPyramid(const std::string &output_dir) const;

private:
	MappedArray<uint8_t> data;
	std::vector<PyrLevel> levels; //starting pointer for each level.
	std::vector<int> heights;
	std::vector<int> widths;
	std::string cache_path;


	int nLevels();
	std::vector<TileRow> initRows();
	bool buildTiledImages(const std::string &input, const std::string &cache_dir);
	void flushLevels(std::vector<TileRow> &rows);
};

}

#endif // PYRAMID_H

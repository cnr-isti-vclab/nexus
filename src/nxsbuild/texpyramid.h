#ifndef TEXPYRAMID_H
#define TEXPYRAMID_H

#include <QRect>
#include <QString>
#include <QImage>
#include <QTemporaryFile>
#include "meshloader.h"

namespace nx {

class TexAtlas;

//split and store in temporary files large textures for better access
class TexLevel {
public:
	TexAtlas *collection;
	int tex, level;
	int width, height;
	int tilew, tileh;              //how many tiles to make up side.
	//std::vector<qint64> offsets; //where each tile starts in the cache.

	void init(int tex, TexAtlas *c, QImage& texture, int _level);
	bool init(int tex, TexAtlas *c, LoadTexture &texture, int _level);
	QImage read(QRect region);
	void build(TexLevel &parent);
	//void build(QImage img);
};

//manage a pyramid of splitted textured
class TexPyramid {
public:
	TexAtlas *collection;
	std::vector<TexLevel> levels;

	void init(int tex, TexAtlas *c, QImage &texture);
	bool init(int tex, TexAtlas *c, LoadTexture &file);
	QImage read(int level, QRect region);
	void buildLevel(int level);
	void buildAllLevels(int n_levels);
};

//Collection of tex pyramids, level 0 is biggest one (bottom).

class TexAtlas {
public:
	struct Index {
		int tex;
		int level;
		int index;
		Index() {}
		Index(int t, int l, int i): tex(t), level(l), index(i) {}
		bool operator<(const Index &i) const {
			if(tex == i.tex) {
				if(level == i.level)
					return index < i.index;
				return level < i.level;
			}
			return tex < i.tex;
		}
		bool operator==(const Index &i) {
			return level == i.level && index == i.index && tex == i.tex;
		}
	};
	struct RamData {
		QImage image;
		uint32_t access;
		RamData() {}
		RamData(QImage img, uint32_t a): image(img), access(a) {}
	};
	struct DiskData {
		uint64_t offset;
		uint64_t size;
		uint32_t w,h;
	};

	const int side = 4096;
	std::vector<TexPyramid> pyramids;
	float scale = 0.70710678;
	int quality = 92;
	uint64_t cache_max = 2000000000;
	uint64_t cache_size = 0;
	uint64_t access = 1;

	TexAtlas() {}

	void addTextures(std::vector<QImage>& textures);
	//will actually fill width and height information
	bool addTextures(std::vector<LoadTexture> &filenames);
	QImage read(int tex, int level, QRect region);
	void buildLevel(int level);

	int width(int tex, int level) { return pyramids[tex].levels[level].width; }
	int height(int tex, int level) { return pyramids[tex].levels[level].height; }
	void flush(int level);
	void pruneCache();
	void addImg(Index index, QImage img);
	QImage getImg(Index index);

	std::map<Index, RamData> ram;
	std::map<Index, DiskData> disk;

	QTemporaryFile storage;
};

} //namespace
#endif // TEXPYRAMID_H


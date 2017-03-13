#ifndef TEXPYRAMID_H
#define TEXPYRAMID_H

#include <QRect>
#include <QString>
#include <QImage>
#include <QImageReader>
#include <QTemporaryFile>

namespace nx {

class TexAtlas;

//split and store in temporary files large textures for better access
class TexLevel {
public:
	TexAtlas *collection;
	int tex, level;
	int width, height;
	int tilew, tileh;              //how many tiles to make up side.
	std::vector<qint64> offsets; //where each tile starts in the cache.

	bool init(int tex, TexAtlas *c, QImageReader &reader);
	QImage read(QRect region);
	void build(TexLevel &parent);

};

//manage a pyramid of splitted textured
class TexPyramid {
public:
	TexAtlas *collection;
	std::vector<TexLevel> levels;

	bool init(int tex, TexAtlas *c, const QString &file);
	QImage read(int level, QRect region);
	void buildLevel(int level);
};

//Collection of tex pyramids, level 0 is biggest one (bottom).

class TexAtlas {
public:
	struct Index {
		int tex;
		int level;
		int index;
		uint32_t access; //record last access time.
		Index() {}
		Index(int t, int l, int i, int a = 0): tex(t), level(l), index(i), access(a) {}
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

	const int side = 1024;
	std::vector<TexPyramid> pyramids;
	float scale;
	int quality;

	TexAtlas(): scale(M_SQRT1_2), quality(92), cache_max(1<<30), cache_size(0), access(1) {}

	bool addTextures(std::vector<QString> &filenames);
	QImage read(int tex, int level, QRect region);
	void buildLevel(int level);

	int width(int tex, int level) { return pyramids[tex].levels[level].width; }
	int height(int tex, int level) { return pyramids[tex].levels[level].height; }
	void flush(int level);
	void pruneCache();
	void cacheImg(Index index, QImage img);

	std::map<Index, QImage> cache;
	uint64_t cache_max;
	uint64_t cache_size;
	uint64_t access;
	QTemporaryFile storage;
};

} //namespace
#endif // TEXPYRAMID_H


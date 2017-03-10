#ifndef TEXPYRAMID_H
#define TEXPYRAMID_H

#include <QRect>
#include <QString>
#include <QImage>
#include <QImageReader>
#include <QTemporaryFile>

namespace nx {

//split and store in temporary files large textures for better access
class TexLevel {
	const int side = 1024;
	int width, height;
	int tilew, tileh;              //how many tiles to make up side.
	std::vector<uint32_t> offsets; //where each tile starts in the cache.
};

//manage a pyramid of splitted textured
class TexPyramid {
public:
	std::vector<TexLevel> levels;
};

//Collection of tex pyramids, level 0 is biggest one (bottom).
class TexCollection {
public:
	std::vector<TexPyramid> pyramids;
	float scale;

	TexCollection(): scale(M_SQRT1_2) {}

	bool addTextures(std::vector<QString> &filenames);
	QImage read(int tex, int level, QRect region);

	int width(int tex, int level) { return pyramids[tex].levels[level].width; }
	int height(int tex, int level) { return pyramids[tex].levels[level].height; }

protected:

	struct Index {
		int tex;
		int level;
		int index;
		int access; //record last access time.
	};

	std::map<Index, QImage> cache;
	QTemporaryFile storage;
};



} //namespace
#endif // TEXPYRAMID_H


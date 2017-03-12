#ifndef TEXPYRAMID_H
#define TEXPYRAMID_H

#include <QRect>
#include <QString>
#include <QImage>
#include <QImageReader>
#include <QTemporaryFile>

namespace nx {

class TexCollection;

//split and store in temporary files large textures for better access
class TexLevel {
public:
	TexCollection *collection;
	int tex, level;
	int width, height;
	int tilew, tileh;              //how many tiles to make up side.
	std::vector<qint64> offsets; //where each tile starts in the cache.

	bool init(int tex, TexCollection *c, QImageReader &reader);
	QImage read(QRect region);
};

//manage a pyramid of splitted textured
class TexPyramid {
public:
	TexCollection *collection;
	std::vector<TexLevel> levels;

	bool init(int tex, TexCollection *c, const QString &file);
	QImage read(int level, QRect region);
};

//Collection of tex pyramids, level 0 is biggest one (bottom).
class TexCollection {
public:
	const int side = 1024;
	std::vector<TexPyramid> pyramids;
	float scale;
	int quality;

	TexCollection(): scale(M_SQRT1_2), quality(92) {}

	bool addTextures(std::vector<QString> &filenames);
	QImage read(int tex, int level, QRect region);

	int width(int tex, int level) { return pyramids[tex].levels[level].width; }
	int height(int tex, int level) { return pyramids[tex].levels[level].height; }
	void flush(int level);


	struct Index {
		int tex;
		int level;
		int index;
		int access; //record last access time.
		Index() {}
		Index(int t, int l, int i, int a = 0): tex(level), level(l), index(i), access(a) {}
		bool operator<(const Index &i) {
			if(tex = i.tex) {
				if(level == i.level)
					return index < i.index;
				return level < i.level;
			}
			return tex < i.tex;
		}
	};

	std::map<Index, QImage> cache;
	QTemporaryFile storage;
};



} //namespace
#endif // TEXPYRAMID_H


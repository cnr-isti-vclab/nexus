#include "texpyramid.h"

using namespace nx;
using namespace std;

bool TexLevel::init(int t, TexCollection *c, QImageReader &reader) {
	tex = t;
	level = 0;
	collection = c;
	int side = collection->side;
	width = reader.size().width();
	height = reader.size().height();

	tilew = 1 + floor(width/side);
	tileh = 1 + floor(height/side);
	offsets.resize(tilew*tileh + 1);
	for(int y = 0; y < tileh; y++) {
		for(int x = 0; x < tilew; x++) {
			int sx = x*side;
			int wx = (sx + side > width)? width - sx : side;
			int sy = y*side;
			int wy = (sy + side > width)? width - sy : side;
			reader.setClipRect(QRect(sx, sy, wx, wy));
			QImage img = reader.read();
			if(img.isNull()) return false;
			offsets[y*tilew + x] = collection->storage.pos();
			img.save(&(collection->storage), "jpg", collection->quality);
		}
	}
	offsets.back() = collection->storage.pos();
	return true;
}

QImage TexLevel::read(QRect region) {
	int side = collection->side;

	int sx = region.x()/side;
	int sy = region.y()/side;
	int ex = (region.x() + region.width() - 1)/side; //last pixel needed
	int ey = (region.y() + region.height() - 1)/side;

	QImage image(region.size(), QImage::Format_RGB32);

	for(int y = sy; y <= ey; y++) {
		for(int x = sx; x <= ex; x++) {
			int index = sx + sy * tilew;

			auto it = collection->cache.find(TexCollection::Index(tex, level, index));
			if(!it) {
				uint64_t size = offsets[index+1] - offsets[index];
				uchar *data = file.map(offsets[index], size);
				QImage img;
				img.loadFromData(data, size);
				//copy into image
				//store into cache.
				//remove excess data from cache
			}
		}
	}
}

bool TexPyramid::init(int tex, TexCollection *c, const QString &file) {
	collection = c;
	QImageReader reader(file);
	if(!reader.canRead()) return false;

	//create level zero.
	levels.resize(1);
	TexLevel &level = levels.back();
	level.init(tex, collection, reader);
}

QImage TexPyramid::read(int level, QRect region) {
	return levels[level].read(tex, level, region);
}



bool TexCollection::addTextures(std::vector<QString> &filenames) {
	pyramids.resize(filenames.size());
	for(int i = 0; i < pyramids.size(); i++) {
		TexPyramid &py = pyramids[i];
		py.quality = quality;
		bool ok = py.init(i, this, filenames[i]);
		if(!ok) return false;
	}
}

QImage TexCollection::read(int tex, int level, QRect region) {
	return pyramids[tex].read(level, region);
}

void TexCollection::flush(int level) {
	for (auto it = cache.cbegin(); it != cache.cend();) {
		if (*it.level == level)
			it = m.erase(it);
		else
			++it;
	}
}



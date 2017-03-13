#include <assert.h>
#include <iostream>
#include <QPainter>
#include <QDebug>
#include "texpyramid.h"

using namespace nx;
using namespace std;

//TODO use RGB format to save some memory
//TODO pass path instead of reader (which cannot be reused
//TODO write back to storage only if loading large images

bool TexLevel::init(int t, TexAtlas *c, QImageReader &reader) {
	tex = t;
	level = 0;
	collection = c;
	int side = collection->side;
	width = reader.size().width();
	height = reader.size().height();

	tilew = (width-1)/side;
	tileh = (height-1)/side;
	offsets.resize(tilew*tileh + 1);
	for(int y = 0; y < tileh; y++) {
		for(int x = 0; x < tilew; x++) {
			int sx = x*side;
			int wx = (sx + side > width)? width - sx : side;
			int sy = y*side;
			int wy = (sy + side > height)? height - sy : side;
			QImageReader rereader(reader.fileName());
			//invert y.
			int isy = height - (sy + wy);
			rereader.setClipRect(QRect(sx, isy, wx, wy));
			cout << "Clip: " << sx << " " << isy << " " << wx << " " << wy <<  endl;
//			reader.device()->seek(0);
			QImage img(wx, wy, QImage::Format_RGB32);
			bool ok = rereader.read(&img);
			if(!ok) {
				cout << "Failed reading texture: " << qPrintable(reader.fileName()) << qPrintable(reader.errorString()) <<  endl;
				return false;
			}
			img = img.mirrored();
			offsets[y*tilew + x] = collection->storage.pos();
			img.save(&(collection->storage), "jpg", collection->quality);
			img.save(QString("tree_%1_%2_%3.jpg").arg(level).arg(x).arg(y));
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

	qDebug() << "Region: " << region.x() << " " << region.y() << " " << region.width() << " " << region.height();
	qDebug() << "Sxy: " << sx << " " << sy << " Exy: " << ex << " " << ey;

	QImage image(region.size(), QImage::Format_RGB32);

	QPainter painter(&image);
	for(int y = sy; y <= ey; y++) {
		for(int x = sx; x <= ex; x++) {
			int id = sx + sy * tilew;

			QImage img;
			TexAtlas::Index index(tex, level, id);
			auto it = collection->cache.find(index);
			if(it == collection->cache.end()) {
				uint64_t size = offsets[id+1] - offsets[id];
				uchar *data = collection->storage.map(offsets[id], size);
				img.loadFromData(data, size);
				collection->storage.unmap(data);
				collection->cacheImg(index, img);
			} else {
				img = (*it).second;
			}
			int tx = std::max(0, x*side - region.x());
			int ty = std::max(0, y*side - region.y());
			int sx = std::max(0, region.x() - x*side);
			int sy = std::max(0, region.y() - y*side);
			int w = std::min(region.width() -tx, side - sx);
			int h = std::min(region.height() -ty, side - sy);



			QRect target(tx, ty, w, h);
			QRect source(sx, sy, w, h);
			qDebug() << "Source: " << source << " Target: " << target;
			assert(w > 0);
			assert(h > 0);
			assert(tx >= 0);
			assert(ty >= 0);
			assert(sx >= 0);
			assert(sy >= 0);
			assert(w <= region.width());
			assert(h <= region.height());
			painter.drawImage(target, img, source);
		}
	}
	collection->pruneCache();
	return image;
}
void TexLevel::build(TexLevel &parent) {
	int side = collection->side;
	float scale = collection->scale;
	width = floor(parent.width * scale);
	height = floor(parent.height * scale);

	tilew = 1 + width/side;
	tileh = 1 + height/side;

	float oside = side/scale;

	offsets.resize(tilew*tileh + 1);
	for(int y = 0; y < tileh; y++) {
		for(int x = 0; x < tilew; x++) {
			int w = (x*side + side > width)? width - x*side : side;
			int h = (y*side + side > height)? height - y*side : side;
			int sx = x*oside;
			int sy = y*oside;
			int sw = sx + oside > parent.width ? parent.width - sx: oside;
			int sh = sy + oside > parent.height ? parent.height - sy: oside;
			QRect region(sx, sy, sw, sh);
			QImage img = parent.read(region);
			img = img.scaled(w, h);
			offsets[y*tilew + x] = collection->storage.pos();
			img.save(&(collection->storage), "jpg", collection->quality);
		}
	}
	offsets.back() = collection->storage.pos();
}

bool TexPyramid::init(int tex, TexAtlas *c, const QString &file) {
	collection = c;
	QImageReader reader(file);
	if(!reader.canRead()) return false;

	//create level zero.
	levels.resize(1);
	TexLevel &level = levels.back();
	return level.init(tex, collection, reader);
}

QImage TexPyramid::read(int level, QRect region) {
	return levels[level].read(region);
}

void TexPyramid::buildLevel(int level) {
	if(levels.size() > (size_t)level) return;
	if(levels.size() != (size_t)level)
		throw "Texture atlas canno skip levels when building...";
	levels.resize(level + 1);
	TexLevel &texlevel = levels.back();
	texlevel.collection = collection;
	texlevel.build(levels[level-1]);
}



bool TexAtlas::addTextures(std::vector<QString> &filenames) {
	pyramids.resize(filenames.size());
	for(size_t i = 0; i < pyramids.size(); i++) {
		TexPyramid &py = pyramids[i];
		bool ok = py.init(i, this, filenames[i]);
		if(!ok) return false;
	}
	return true;
}

QImage TexAtlas::read(int tex, int level, QRect region) {
	return pyramids[tex].read(level, region);
}

void TexAtlas::cacheImg(Index index, QImage img) {
	index.access = access++;
	cache_size += img.width()*img.height()*4;
	cache[index] = img;
}


void TexAtlas::buildLevel(int level) {
	if(!pyramids.size()) return;
	for(auto &py: pyramids)
		py.buildLevel(level);
}

void TexAtlas::flush(int level) {
	for (auto it = cache.cbegin(); it != cache.cend();) {
		if (it->first.level == level) {
			cache_size -= 4*(it->second.width())*(it->second.height());
			it = cache.erase(it);
		} else
			++it;
	}
}

void TexAtlas::pruneCache() {
	while(cache_size > cache_max) {

		Index oldest;
		oldest.access = 0;
		for (auto it = cache.cbegin(); it != cache.cend();) {
			if(it->first.access < oldest.access)
				oldest = it->first;
		}
		auto it = cache.find(oldest);
		cache_size -= 4*(it->second.width())*(it->second.height());
		cache.erase(it);
	}
}



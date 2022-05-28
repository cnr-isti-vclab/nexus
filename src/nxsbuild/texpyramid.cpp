#include <assert.h>
#include <math.h>
#include <iostream>
#include <QPainter>
#include <QImageReader>
#include <QDebug>
#include "texpyramid.h"

using namespace nx;
using namespace std;

void TexLevel::init(int t, TexAtlas* c, const QImage& texture) {
	tex = t;
	level = 0;
	collection = c;
	int side = collection->side;

	width = texture.width();
	height = texture.height();

	tilew = (width-1)/side +1;
	tileh = (height-1)/side +1;

	for(int y = 0; y < tileh; y++) {
		for(int x = 0; x < tilew; x++) {
			int sx = x*side;
			int wx = (sx + side > width)? width - sx : side;
			int sy = y*side;
			int wy = (sy + side > height)? height - sy : side;
			int isy = height - (sy + wy);
			QImage img = texture.copy(QRect(sx, isy, wx, wy));
			img = img.convertToFormat(QImage::Format_RGB32);
			img = img.mirrored();
			collection->addImg(TexAtlas::Index(tex, level, x + y*tilew), img);
		}
	}
}

bool TexLevel::init(int t, TexAtlas *c, LoadTexture &texture) {
	tex = t;
	level = 0;
	collection = c;
	int side = collection->side;
	QImageReader test(texture.filename);
	if(!test.canRead()) return false;

	texture.width = width = test.size().width();
	texture.height = height = test.size().height();

	tilew = (width-1)/side +1;
	tileh = (height-1)/side +1;

	for(int y = 0; y < tileh; y++) {
		for(int x = 0; x < tilew; x++) {
			int sx = x*side;
			int wx = (sx + side > width)? width - sx : side;
			int sy = y*side;
			int wy = (sy + side > height)? height - sy : side;
			QImageReader reader(texture.filename);
			//invert y.
			int isy = height - (sy + wy);
			reader.setClipRect(QRect(sx, isy, wx, wy));

			QImage img(wx, wy, QImage::Format_RGB32);
			bool ok = reader.read(&img);
			if(!ok) {
				cout << "Failed reading texture: " << qPrintable(reader.fileName()) << qPrintable(reader.errorString()) <<  endl;
				return false;
			}
			img = img.mirrored();
			collection->addImg(TexAtlas::Index(tex, level, x + y*tilew), img);
		}
	}
	return true;
}

QImage TexLevel::read(QRect region) {
	int side = collection->side;

	int sx = region.x()/side;
	int sy = region.y()/side;
	int ex = (region.x() + region.width() - 1)/side; //last pixel needed
	int ey = (region.y() + region.height() - 1)/side;

	QImage image(region.size(), QImage::Format_RGB32);

	QPainter painter(&image);
	for(int y = sy; y <= ey; y++) {
		for(int x = sx; x <= ex; x++) {
			int id = x + y * tilew;
			TexAtlas::Index index(tex, level, id);

			QImage img = collection->getImg(index);

			int tx = std::max(0, x*side - region.x()); //target
			int ty = std::max(0, y*side - region.y());
			int ox = std::max(0, region.x() - x*side); //origins
			int oy = std::max(0, region.y() - y*side);
			int w = std::min(region.width() -tx, side - ox);
			int h = std::min(region.height() -ty, side - oy);

			QRect target(tx, ty, w, h);
			QRect source(ox, oy, w, h);
			assert(w > 0   && h > 0);
			assert(tx >= 0 && ty >= 0);
			assert(ox >= 0 && oy >= 0);
			assert(w <= region.width());
			assert(h <= region.height());
			painter.drawImage(target, img, source);
		}
	}
	collection->pruneCache();
	return image;
}
//TODO could we speed up this process?
//in theory we should be able to process the file just once
//reading line by line and fill all the levels at the same time
//keep 2 lines in float for averaging, for each first line of tiles of each level.
//in qimage reader use setscaledcliprect
void TexLevel::build(TexLevel &parent) {
	int side = collection->side;
	float scale = collection->scale;
	tex = parent.tex;
	width = floor(parent.width * scale);
	height = floor(parent.height * scale);

	tilew = 1 + (width-1)/side;
	tileh = 1 + (height-1)/side;

	int oside = (int)(side/scale);

	for(int y = 0; y < tileh; y++) {
		for(int x = 0; x < tilew; x++) {
			int w = (x*side + side > width)? width - x*side : side;
			int h = (y*side + side > height)? height - y*side : side;
			int sx = x*oside;
			int sy = y*oside;
			int sw = (sx + oside > parent.width) ? parent.width - sx: oside;
			int sh = (sy + oside > parent.height) ? parent.height - sy: oside;
			QRect region(sx, sy, sw, sh);
			QImage img = parent.read(region);
			img = img.scaled(w, h);
			collection->addImg(TexAtlas::Index(tex, level, x + tilew*y), img);
		}
	}
}

void TexPyramid::init(int tex, TexAtlas *c, const QImage &texture) {
	collection = c;
	//create level zero.
	levels.resize(1);
	TexLevel &level = levels.back();
	return level.init(tex, collection, texture);
}

bool TexPyramid::init(int tex, TexAtlas *c, LoadTexture &file) {
	collection = c;
	//create level zero.
	levels.resize(1);
	TexLevel &level = levels.back();
	return level.init(tex, collection, file);
}

QImage TexPyramid::read(int level, QRect region) {
	return levels[level].read(region);
}

void TexPyramid::buildLevel(int level) {
	if(levels.size() > (size_t)level) return;
	if(levels.size() != (size_t)level)
		throw QString("texture atlas cannot skip levels when building");
	levels.resize(level + 1);
	TexLevel &texlevel = levels.back();
	texlevel.level = level;
	texlevel.collection = collection;
	texlevel.build(levels[level-1]);
}



void TexAtlas::addTextures(const std::vector<QImage>& textures) {
	pyramids.resize(textures.size());
	for(size_t i = 0; i < pyramids.size(); i++) {
		TexPyramid &py = pyramids[i];
		py.init(i, this, textures[i]);
	}
}

bool TexAtlas::addTextures(std::vector<LoadTexture> &textures) {
	pyramids.resize(textures.size());
	for(size_t i = 0; i < pyramids.size(); i++) {
		TexPyramid &py = pyramids[i];
		bool ok = py.init(i, this, textures[i]);
		if(!ok) {
			throw ("could not load texture: " + textures[i].filename);
		}
	}
	return true;
}

QImage TexAtlas::read(int tex, int level, QRect region) {
	return pyramids[tex].read(level, region);
}

void TexAtlas::addImg(Index index, QImage img) {
	//cout << "Adding: tex: " << index.tex << " level: " << index.level << " index: " << index.index << endl;
	cache_size += img.width()*img.height()*4;
	ram[index] = RamData(img, access++);
	pruneCache();
}

QImage TexAtlas::getImg(Index index) {
	auto it = ram.find(index);
	if(it != ram.end())
		return it->second.image;

	auto dt = disk.find(index);
	if(dt == disk.end())
		throw QString("unespected missing image in disk and ram");

	QImage img(dt->second.w, dt->second.h, QImage::Format_RGB32);
	uchar *data = storage.map(dt->second.offset, dt->second.size);
	img.loadFromData(data, dt->second.size);
	storage.unmap(data);
	addImg(index, img);
	return img;
}



void TexAtlas::buildLevel(int level) {
	if(!pyramids.size()) return;
	for(auto &py: pyramids)
		py.buildLevel(level);
}

//do not store it in temporary file, we are throwing away this level.
void TexAtlas::flush(int level) {
	for (auto it = ram.cbegin(); it != ram.cend();) {
		if (it->first.level == level) {
			cache_size -= 4*(it->second.image.width())*(it->second.image.height());
			it = ram.erase(it);
		} else
			++it;
	}
}

void TexAtlas::pruneCache() {
	while(cache_size > cache_max) {
		Index index;
		uint32_t oldest = access;
		for (auto it = ram.cbegin(); it != ram.cend(); it++) {
			if(it->second.access < oldest) {
				index = it->first;
				oldest = it->second.access;
			}
		}
		auto it = ram.find(index);
		cache_size -= 4*(it->second.image.width())*(it->second.image.height());
		if(disk.find(index) == disk.end()) {
			DiskData d;
			d.offset = storage.pos();
			d.w = it->second.image.width();
			d.h = it->second.image.height();
			it->second.image.save(&storage, "jpg", quality);
			d.size = storage.pos() - d.offset;
			disk[index] = d;
		}
		ram.erase(it);
	}
}



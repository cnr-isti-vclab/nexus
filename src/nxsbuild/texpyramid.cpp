#include "texpyramid.h"

using namespace nx;
using namespace std;

bool addTextures(std::vector<QString> &filenames) {
	pyramids.resize(filenames.size());
	for(int i = 0; i < pyramids.size(); i++) {
		TexPyramid &py = pyramids[i];
		py.init(filenames, cache);
	}
}

QImage read(int tex, int level, QRect region) {

}

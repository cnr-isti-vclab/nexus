#include "colormap.h"

#include <map>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <math.h>

#include <iostream>
#define clamp(v, m, M) std::min(M, std::max(m, v));

using namespace std;

bool Colormap::loadFromCSV(const string& filename) {
	ifstream file(filename);
	if (!file.is_open()) return false;

	colormap.clear();
	string line;
	while (getline(file, line)) {
		stringstream ss(line);
		Color color;
		for (float& c : color) {
			string value;
			if (!getline(ss, value, ',')) return false;
			c = stof(value);
		}
		colormap.push_back(color);
	}
	return !colormap.empty();
}

bool Colormap::setColormap(const string& name) {
	static const std::map<string, vector<Color>> predefinedColormaps = {
		{"viridis", {{0.267f, 0.004f, 0.329f, 1.0f}, {0.283f, 0.141f, 0.458f, 1.0f}, {0.254f, 0.265f, 0.530f, 1.0f}, {0.207f, 0.372f, 0.553f, 1.0f}, {0.164f, 0.471f, 0.558f, 1.0f}, {0.128f, 0.567f, 0.551f, 1.0f}, {0.135f, 0.659f, 0.518f, 1.0f}, {0.267f, 0.850f, 0.441f, 1.0f}, {0.477f, 1.000f, 0.318f, 1.0f}}},
		{"spectral", {{0.619f, 0.003f, 0.258f, 1.0f}, {0.835f, 0.243f, 0.310f, 1.0f}, {0.957f, 0.427f, 0.263f, 1.0f}, {0.992f, 0.682f, 0.380f, 1.0f}, {1.000f, 0.878f, 0.545f, 1.0f}, {0.878f, 1.000f, 0.392f, 1.0f}, {0.671f, 0.866f, 0.204f, 1.0f}, {0.471f, 0.671f, 0.059f, 1.0f}, {0.267f, 0.459f, 0.000f, 1.0f}}},
		{"plasma", {{0.050f, 0.029f, 0.529f, 1.0f}, {0.294f, 0.001f, 0.635f, 1.0f}, {0.492f, 0.012f, 0.603f, 1.0f}, {0.676f, 0.209f, 0.486f, 1.0f}, {0.830f, 0.369f, 0.326f, 1.0f}, {0.959f, 0.566f, 0.152f, 1.0f}, {0.987f, 0.788f, 0.071f, 1.0f}, {0.941f, 0.975f, 0.320f, 1.0f}}}
	};

	auto it = predefinedColormaps.find(name);
	if (it == predefinedColormaps.end()) {
		return false;
	}
	colormap = it->second;
	cout << "Returning true" << endl;
	return true;
}

Colormap::Colorb Colormap::map(float value) const {
	value = clamp((value - minValue) / (maxValue - minValue), 0.0f, 1.0f);
	float scaled = value * (colormap.size() - 1);
	size_t idx = static_cast<size_t>(scaled);
	float t = scaled - idx;

	Color c;
	if (idx >= colormap.size() - 1)
		c = colormap.back();
	else
		c = interpolate(colormap[idx], colormap[idx + 1], t);

	Colorb cb;
	for(int i = 0; i < 4; i++) {
		cb[i] = static_cast<unsigned char>(floor(c[i]*255));
	}
	return cb;
}


Colormap::Color Colormap::interpolate(const Color& c1, const Color& c2, float t) {
	Color result;
	for (size_t i = 0; i < 4; ++i) {
		result[i] = c1[i] * (1.0f - t) + c2[i] * t;
	}
	return result;
}

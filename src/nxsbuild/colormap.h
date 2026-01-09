#ifndef COLORMAP_H
#define COLORMAP_H


#include <vector>
#include <array>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <stdexcept>


class Colormap {
public:
	using Color = std::array<float, 4>; // RGBA
	using Colorb = std::array<unsigned char, 4>;

	float minValue, maxValue;
	std::vector<Color> colormap;

	Colormap(float min = 0.0f, float max = 1.0f) : minValue(min), maxValue(max) {}

	void setColormap(const std::vector<Color>& colors) { colormap = colors; }
	bool setColormap(const std::string& name);
	bool loadFromCSV(const std::string& filename);

	Colorb map(float value) const;

	static Color interpolate(const Color& c1, const Color& c2, float t);
};


#endif // COLORMAP_H

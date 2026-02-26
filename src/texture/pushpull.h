#ifndef __PUSHPULL_H__
#define __PUSHPULL_H__

#include <cstdint>
#include <vector>

#include "../core/material.h"

namespace nx {

void pushPullFillUnwrittenPixels(int width,
	int height,
	std::vector<uint8_t>& texels,
	const std::vector<uint8_t>& mask,
	const std::vector<Material>& materials);

}

#endif /* __PUSHPULL_H__ */

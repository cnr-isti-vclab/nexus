#ifndef NX_TEXTURE_CACHE_H
#define NX_TEXTURE_CACHE_H

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "pyramid.h"
#include "../core/material.h"

namespace nx {

class TextureCache {
public:
	explicit TextureCache(std::string cache_dir);

	void initFromMaterials(const std::vector<Material>& materials);
	const Pyramid* get(const std::string& path);
	Vector3f sample(const std::string& path, const Vector2f& uv, int level = 0);

private:
	std::string cache_dir_;
	std::unordered_map<std::string, std::unique_ptr<Pyramid>> cache_;
};

} // namespace nx

#endif // NX_TEXTURE_CACHE_H

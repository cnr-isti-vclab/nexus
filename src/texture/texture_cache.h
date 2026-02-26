#ifndef NX_TEXTURE_CACHE_H
#define NX_TEXTURE_CACHE_H

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "pyramid.h"
#include "../core/material.h"

namespace nx {

//Levels should be reversed in the pyramid. 0 => base, now 0 => top.

class TextureCache {
public:
	explicit TextureCache(std::string cache_dir);

	void initFromMaterials(std::vector<Material>& materials);
	Material::TextureId getId(const std::string& path);
	const Pyramid* get(Material::TextureId id) const;
	const Pyramid* get(const std::string& path);
	Vector3f sample(Material::TextureId id, const Vector2f& uv, int level = -1) const;
	Vector3f sample(const std::string& path, const Vector2f& uv, int level = -1);


private:
	std::string cache_dir_;
	std::vector<std::unique_ptr<Pyramid>> cache_;
	std::unordered_map<std::string, Material::TextureId> path_to_id_;
};

} // namespace nx

#endif // NX_TEXTURE_CACHE_H

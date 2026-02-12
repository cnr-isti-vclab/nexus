#include "texture_cache.h"

#include <cassert>

namespace nx {

TextureCache::TextureCache(std::string cache_dir)
	: cache_dir_(std::move(cache_dir)) {}

void TextureCache::initFromMaterials(const std::vector<Material>& materials) {
	auto preload = [this](const std::string& path) {
		if (!path.empty()) {
			(void)get(path);
		}
	};

	for (const Material& material : materials) {
		preload(material.base_color_texture);
		preload(material.metallic_roughness_texture);
		preload(material.specular_texture);
		preload(material.normal_texture);
		preload(material.occlusion_texture);
		preload(material.emissive_texture);
	}
}

const Pyramid* TextureCache::get(const std::string& path) {
	if (path.empty()) {
		return nullptr;
	}

	auto it = cache_.find(path);
	if (it != cache_.end()) {
		return it->second.get();
	}

	auto pyramid = std::make_unique<Pyramid>();
	bool ok = pyramid->build(path, cache_dir_);
	assert(ok);
	if (!ok) {
		return nullptr;
	}

	const Pyramid* result = pyramid.get();
	cache_.emplace(path, std::move(pyramid));
	return result;
}

Vector3f TextureCache::sample(const std::string& path, const Vector2f& uv, int level) {
	const Pyramid* pyramid = get(path);
	if (!pyramid) {
		return {1.0f, 1.0f, 1.0f};
	}
	return pyramid->sample(uv.u, uv.v, level);
}

} // namespace nx

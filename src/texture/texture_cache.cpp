#include "texture_cache.h"

#include <cassert>

namespace nx {

TextureCache::TextureCache(std::string cache_dir)
	: cache_dir_(std::move(cache_dir)) {}

void TextureCache::initFromMaterials(std::vector<Material>& materials) {
	for (Material& material : materials) {
		for (size_t i = 0; i < Material::kTextureSlotCount; ++i) {
			auto slot = static_cast<Material::TextureSlot>(i);
			material.texture_ids[i] = getId(material.texturePath(slot));
		}
	}
}

Material::TextureId TextureCache::getId(const std::string& path) {
	if (path.empty()) {
		return Material::INVALID_TEXTURE_ID;
	}

	auto it = path_to_id_.find(path);
	if (it != path_to_id_.end()) {
		return it->second;
	}

	auto pyramid = std::make_unique<Pyramid>();
	pyramid->build(path, cache_dir_);

	Material::TextureId id = static_cast<Material::TextureId>(cache_.size());
	cache_.push_back(std::move(pyramid));
	path_to_id_.emplace(path, id);
	return id;
}

const Pyramid* TextureCache::get(Material::TextureId id) const {
	if (id == Material::INVALID_TEXTURE_ID) {
		return nullptr;
	}
	assert(id >= 0);
	assert(static_cast<size_t>(id) < cache_.size());

	return cache_[static_cast<size_t>(id)].get();
}

const Pyramid* TextureCache::get(const std::string& path) {
	Material::TextureId id = getId(path);
	return get(id);
}

Vector3f TextureCache::sample(Material::TextureId id, const Vector2f& uv, int level) const {
	const Pyramid* pyramid = get(id);
	if (!pyramid) {
		return {1.0f, 1.0f, 1.0f};
	}
	return pyramid->sample(uv.u, uv.v, level);
}

Vector3f TextureCache::sample(const std::string& path, const Vector2f& uv, int level) {
	Material::TextureId id = getId(path);
	return sample(id, uv, level);
}

} // namespace nx

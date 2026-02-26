#ifndef NX_MATERIAL_H
#define NX_MATERIAL_H

#include <cstdint>
#include <cassert>
#include <string>
#include <filesystem>
#include <array>
#include <vector>
#include <math.h>

namespace nx {

enum class MaterialType : uint8_t {
	PBR = 0,
	PHONG = 1
};

/**
 * Material supporting both PBR (glTF metallic-roughness) and legacy Phong shading.
 * 
 * PBR workflow:
 *   - Use base_color, metallic_factor, roughness_factor
 *   - Optional textures: base_color_texture, metallic_roughness_texture
 * 
 * Phong simulation (legacy OBJ/MTL):
 *   - diffuse (Kd) → base_color, base_color_texture
 *   - specular (Ks) → simulated via low metallic (0.0) + low roughness
 *   - shininess (Ns) → mapped to roughness: roughness ≈ sqrt(2/(Ns+2))
 *   - ambient (Ka) → can be baked into occlusion or ignored
 * 
 * Texture packing (glTF):
 *   - metallic_roughness_texture: R=occlusion, G=roughness, B=metallic
 */

struct Material {
	enum class TextureSlot : uint8_t {
		BaseColor = 0,
		MetallicRoughness,
		Specular,
		Normal,
		Occlusion,
		Emissive,
		Count
	};

	using TextureId = int32_t;
	static constexpr TextureId INVALID_TEXTURE_ID = -1;
	static constexpr size_t kTextureSlotCount = static_cast<size_t>(TextureSlot::Count);

	static constexpr size_t slotIndex(TextureSlot slot) {
		return static_cast<size_t>(slot);
	}

	MaterialType type = MaterialType::PBR;

	// Base color / diffuse (Kd in Phong) - RGBA
	float base_color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
	std::string base_color_texture;

	// Metallic-roughness (PBR)
	float metallic_factor = 1.0f;   // 0.0 = dielectric, 1.0 = metal
	float roughness_factor = 1.0f;  // 0.0 = smooth, 1.0 = rough
	std::string metallic_roughness_texture; // G=roughness, B=metallic

	// Specular (Phong Ks) - for legacy materials; ignored in pure PBR
	float specular[3] = {0.0f, 0.0f, 0.0f};
	std::string specular_texture;
	float shininess = 0.0f; // Phong Ns; convert to roughness if needed

	// Normal mapping
	std::string normal_texture;

	float normal_scale = 1.0f;

	// Occlusion
	std::string occlusion_texture;
	float occlusion_strength = 1.0f;

	// Emissive - RGB
	float emissive_factor[3] = {0.0f, 0.0f, 0.0f};
	std::string emissive_texture;

	// Rendering flags
	bool double_sided = false;

	// Optional name/id
	std::string name;

	std::array<TextureId, kTextureSlotCount> texture_ids = {
		INVALID_TEXTURE_ID,
		INVALID_TEXTURE_ID,
		INVALID_TEXTURE_ID,
		INVALID_TEXTURE_ID,
		INVALID_TEXTURE_ID,
		INVALID_TEXTURE_ID
	};

	// Helpers
	bool has_base_color_texture()         const { return !base_color_texture.empty(); }
	bool has_metallic_roughness_texture() const { return !metallic_roughness_texture.empty(); }
	bool has_specular_texture()           const { return !specular_texture.empty(); }
	bool has_normal_texture()             const { return !normal_texture.empty(); }
	bool has_occlusion_texture()          const { return !occlusion_texture.empty(); }
	bool has_emissive_texture()           const { return !emissive_texture.empty(); }


	std::string& texturePath(TextureSlot slot) {
		switch (slot) {
		case TextureSlot::BaseColor:         return base_color_texture;
		case TextureSlot::MetallicRoughness: return metallic_roughness_texture;
		case TextureSlot::Specular:          return specular_texture;
		case TextureSlot::Normal:            return normal_texture;
		case TextureSlot::Occlusion:         return occlusion_texture;
		case TextureSlot::Emissive:          return emissive_texture;
		default:
			assert(false && "Invalid material texture slot");
			return base_color_texture;
		}
	}

	bool hasTextureId(TextureSlot slot) const {
		return texture_ids[slotIndex(slot)] != INVALID_TEXTURE_ID;
	}

	bool is_phong() const { return type == MaterialType::PHONG; }
	bool is_pbr() const { return type == MaterialType::PBR; }

	// Convert Phong shininess to PBR roughness (approximation)
	static float shininess_to_roughness(float ns) {
		return (ns > 0.0f) ? sqrt(2.0f / (ns + 2.0f)) : 1.0f;
	}
};

struct TileMap {
	int width = 0;
	int height = 0;

	std::vector<Material::TextureSlot> texture_slots; //which maps are stored in texels one after the other.
	std::vector<int> slot_offsets = std::vector<int>(Material::kTextureSlotCount, -1); // indexed by Material::slotIndex(slot)
	std::vector<uint8_t> texels;

	void addSlot(Material::TextureSlot slot);
	uint8_t *pixel(int x, int y, Material::TextureSlot slot) {
		size_t idx = Material::slotIndex(slot);
		assert(idx < slot_offsets.size());
		assert(slot_offsets[idx] >= 0);
		return &texels[slot_offsets[idx] + 3*(x + y*width)];
	}
};

} // namespace nx

#endif // NX_MATERIAL_H

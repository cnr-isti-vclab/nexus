#ifndef NX_MATERIAL_H
#define NX_MATERIAL_H

#include <cstdint>
#include <string>
#include <filesystem>
#include <math.h>
#include "../texture/pyramid.h"

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
	MaterialType type = MaterialType::PBR;

	// Base color / diffuse (Kd in Phong) - RGBA
	float base_color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
	std::string base_color_texture;
	Pyramid* base_color_map = nullptr;

		// Metallic-roughness (PBR)
		float metallic_factor = 1.0f;   // 0.0 = dielectric, 1.0 = metal
	float roughness_factor = 1.0f;  // 0.0 = smooth, 1.0 = rough
	std::string metallic_roughness_texture; // G=roughness, B=metallic
		Pyramid* metallic_roughness_map = nullptr;

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
	Pyramid* emissive_id_map = nullptr;

	// Rendering flags
	bool double_sided = false;

	// Optional name/id
	std::string name;

	Material() = default;
	Material(const Material& other);
	Material& operator=(const Material& other);
	Material(Material&& other) noexcept;
	Material& operator=(Material&& other) noexcept;
	~Material();

	void initPyramids(const std::string& cache_dir);
	void destroyPyramids();

	// Helpers
	bool has_base_color_texture() const { return !base_color_texture.empty(); }
	bool has_metallic_roughness_texture() const { return !metallic_roughness_texture.empty(); }
	bool has_specular_texture() const { return !specular_texture.empty(); }
	bool has_normal_texture() const { return !normal_texture.empty(); }
	bool has_occlusion_texture() const { return !occlusion_texture.empty(); }
	bool has_emissive_texture() const { return !emissive_texture.empty(); }
	bool is_phong() const { return type == MaterialType::PHONG; }
	bool is_pbr() const { return type == MaterialType::PBR; }

	// Convert Phong shininess to PBR roughness (approximation)
	static float shininess_to_roughness(float ns) {
		return (ns > 0.0f) ? sqrt(2.0f / (ns + 2.0f)) : 1.0f;
	}
};

} // namespace nx

#endif // NX_MATERIAL_H

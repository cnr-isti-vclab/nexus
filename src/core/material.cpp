#include "material.h"
#include <cmath>

namespace nx {

// Compare floats with tolerance (1/255 for color precision)
static bool same_parameter(float a, float b) {
    return std::fabs(a - b) < 1.0f / 255.0f;
}

static bool same_color3(const float* a, const float* b) {
    return same_parameter(a[0], b[0]) && 
           same_parameter(a[1], b[1]) && 
           same_parameter(a[2], b[2]);
}

static bool same_color4(const float* a, const float* b) {
    return same_color3(a, b) && same_parameter(a[3], b[3]);
}

// Check if two materials have texture in the same slot (regardless of actual path)
static bool same_texture_role(const std::string& a, const std::string& b) {
    return (a.empty() && b.empty()) || (!a.empty() && !b.empty());
}

/**
 * Check if two materials can be merged (same parameters and texture roles).
 * Materials with same roles can share an atlas even if texture paths differ.
 */
bool can_merge_materials(const Material& a, const Material& b) {
    // Type must match
    if (a.type != b.type) return false;
    
    // Base color and texture role
    if (!same_color4(a.base_color, b.base_color)) return false;
	if (!same_texture_role(a.base_color_texture, b.base_color_texture)) return false;
    
    // PBR parameters
    if (!same_parameter(a.metallic_factor, b.metallic_factor)) return false;
    if (!same_parameter(a.roughness_factor, b.roughness_factor)) return false;
    if (!same_texture_role(a.metallic_roughness_texture, b.metallic_roughness_texture)) return false;
    
    // Phong parameters (if Phong)
    if (a.is_phong()) {
        if (!same_color3(a.specular, b.specular)) return false;
        if (!same_parameter(a.shininess, b.shininess)) return false;
        if (!same_texture_role(a.specular_texture, b.specular_texture)) return false;
    }
    
    // Normal mapping
    if (!same_parameter(a.normal_scale, b.normal_scale)) return false;
    if (!same_texture_role(a.normal_texture, b.normal_texture)) return false;
    
    // Occlusion
    if (!same_parameter(a.occlusion_strength, b.occlusion_strength)) return false;
    if (!same_texture_role(a.occlusion_texture, b.occlusion_texture)) return false;
    
    // Emissive
    if (!same_color3(a.emissive_factor, b.emissive_factor)) return false;
    if (!same_texture_role(a.emissive_texture, b.emissive_texture)) return false;
    
    // Rendering flags
    if (a.double_sided != b.double_sided) return false;
    
    return true;
}

Material::Material(const Material& other)
    : type(other.type),
      base_color_texture(other.base_color_texture),
      metallic_factor(other.metallic_factor),
      roughness_factor(other.roughness_factor),
      metallic_roughness_texture(other.metallic_roughness_texture),
      specular_texture(other.specular_texture),
      shininess(other.shininess),
      normal_texture(other.normal_texture),
      normal_scale(other.normal_scale),
      occlusion_texture(other.occlusion_texture),
      occlusion_strength(other.occlusion_strength),
      emissive_texture(other.emissive_texture),
      double_sided(other.double_sided),
      name(other.name) {
    std::copy(other.base_color, other.base_color + 4, base_color);
    std::copy(other.specular, other.specular + 3, specular);
    std::copy(other.emissive_factor, other.emissive_factor + 3, emissive_factor);
}

Material& Material::operator=(const Material& other) {
    if (this == &other) {
        return *this;
    }
    type = other.type;
    base_color_texture = other.base_color_texture;
    metallic_factor = other.metallic_factor;
    roughness_factor = other.roughness_factor;
    metallic_roughness_texture = other.metallic_roughness_texture;
    specular_texture = other.specular_texture;
    shininess = other.shininess;
    normal_texture = other.normal_texture;
    normal_scale = other.normal_scale;
    occlusion_texture = other.occlusion_texture;
    occlusion_strength = other.occlusion_strength;
    emissive_texture = other.emissive_texture;
    double_sided = other.double_sided;
    name = other.name;
    std::copy(other.base_color, other.base_color + 4, base_color);
    std::copy(other.specular, other.specular + 3, specular);
    std::copy(other.emissive_factor, other.emissive_factor + 3, emissive_factor);
    return *this;
}

Material::Material(Material&& other) noexcept
    : type(other.type),
      base_color_texture(std::move(other.base_color_texture)),
      metallic_factor(other.metallic_factor),
      roughness_factor(other.roughness_factor),
      metallic_roughness_texture(std::move(other.metallic_roughness_texture)),
      specular_texture(std::move(other.specular_texture)),
      shininess(other.shininess),
      normal_texture(std::move(other.normal_texture)),
      normal_scale(other.normal_scale),
      occlusion_texture(std::move(other.occlusion_texture)),
      occlusion_strength(other.occlusion_strength),
      emissive_texture(std::move(other.emissive_texture)),
      double_sided(other.double_sided),
      name(std::move(other.name)) {
    std::copy(other.base_color, other.base_color + 4, base_color);
    std::copy(other.specular, other.specular + 3, specular);
    std::copy(other.emissive_factor, other.emissive_factor + 3, emissive_factor);
}

Material& Material::operator=(Material&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    type = other.type;
    base_color_texture = std::move(other.base_color_texture);
    metallic_factor = other.metallic_factor;
    roughness_factor = other.roughness_factor;
    metallic_roughness_texture = std::move(other.metallic_roughness_texture);
    specular_texture = std::move(other.specular_texture);
    shininess = other.shininess;
    normal_texture = std::move(other.normal_texture);
    normal_scale = other.normal_scale;
    occlusion_texture = std::move(other.occlusion_texture);
    occlusion_strength = other.occlusion_strength;
    emissive_texture = std::move(other.emissive_texture);
    double_sided = other.double_sided;
    name = std::move(other.name);
    std::copy(other.base_color, other.base_color + 4, base_color);
    std::copy(other.specular, other.specular + 3, specular);
    std::copy(other.emissive_factor, other.emissive_factor + 3, emissive_factor);
    return *this;
}

Material::~Material() = default;

} // namespace nx

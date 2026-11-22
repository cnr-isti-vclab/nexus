#include "signature.h"

#include <cstddef>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace nx {
namespace detail {

using StridedCopy = void (*)(char *, std::size_t, const char *, std::size_t, uint32_t, std::size_t);

template <std::size_t Bytes>
inline void copyStridedFixed(char *dst, std::size_t dstStride,
                             const char *src, std::size_t srcStride,
                             uint32_t count, std::size_t /*unused*/) {
	for (uint32_t v = 0; v < count; ++v) {
		std::memcpy(dst, src, Bytes);
		dst += dstStride;
		src += srcStride;
	}
}

inline void copyStridedVariable(char *dst, std::size_t dstStride,
                                const char *src, std::size_t srcStride,
                                uint32_t count, std::size_t bytes) {
	for (uint32_t v = 0; v < count; ++v) {
		std::memcpy(dst, src, bytes);
		dst += dstStride;
		src += srcStride;
	}
}

inline StridedCopy stridedCopyForSize(std::size_t bytes) {
	switch (bytes) {
	case 4:  return &copyStridedFixed<4>;
	case 8:  return &copyStridedFixed<8>;
	case 12: return &copyStridedFixed<12>;
	case 16: return &copyStridedFixed<16>;
	case 24: return &copyStridedFixed<24>;
	case 32: return &copyStridedFixed<32>;
	default: return &copyStridedVariable;
	}
}

} // namespace detail

void VertexElement::interleave(uint32_t count, const void *const *components, char *destination) const {
	if (!count)
		return;
	if (!components || !destination || !vertex_size)
		throw std::invalid_argument("invalid interleaved vertex buffer");
	if (count > std::numeric_limits<std::size_t>::max() / vertex_size)
		throw std::length_error("interleaved vertex buffer size overflows");

	for (unsigned int c = 0; c < COMPONENT_COUNT; ++c) {
		const int bytes = attributes[c].size();
		if (!bytes)
			continue;
		if (!components[c])
			throw std::invalid_argument("missing source data for vertex component");

		auto copyFn = detail::stridedCopyForSize(bytes);
		copyFn(destination + component_offsets[c], vertex_size,
		       static_cast<const char *>(components[c]), bytes, count, bytes);
	}
}

void VertexElement::deinterleave(uint32_t count, const char *source, void *const *components) const {
	if (!count)
		return;
	if (!source || !components || !vertex_size)
		throw std::invalid_argument("invalid interleaved vertex buffer");
	if (count > std::numeric_limits<std::size_t>::max() / vertex_size)
		throw std::length_error("interleaved vertex buffer size overflows");

	for (unsigned int c = 0; c < COMPONENT_COUNT; ++c) {
		const int bytes = attributes[c].size();
		if (!bytes)
			continue;
		if (!components[c])
			throw std::invalid_argument("missing destination buffer for vertex component");

		auto copyFn = detail::stridedCopyForSize(bytes);
		copyFn(static_cast<char *>(components[c]), bytes,
		       source + component_offsets[c], vertex_size, count, bytes);
	}
}

} // namespace nx

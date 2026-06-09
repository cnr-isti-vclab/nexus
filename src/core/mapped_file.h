#ifndef NX_MAPPED_FILE_H
#define NX_MAPPED_FILE_H

#include <string>
#include <cstdint>
#include <vector>
#include <stdexcept>

namespace nx {


class MappedFile {
public:
	enum Mode {
		READ_ONLY,
		READ_WRITE,
		TEMPORARY // Created in temp, deleted on destruction
	};

	MappedFile();
	~MappedFile();

	// Disable copying
	MappedFile(const MappedFile&) = delete;
	MappedFile& operator=(const MappedFile&) = delete;

	// Move semantics
	MappedFile(MappedFile&& other) noexcept;
	MappedFile& operator=(MappedFile&& other) noexcept;

	bool open(const std::string& filename, Mode mode, size_t size = 0);
	bool open(const std::string& folder, const std::string& prefix, Mode mode, size_t size = 0);
	static std::string makeTempPath(const std::string& folder, const std::string& prefix);
	void close();

	// Pointers obtained via data() become invalid!
	bool resize(size_t new_size);

	void* data() const { return _data; }
	size_t size() const { return _size; }
	bool isValid() const { return _data != nullptr; }

private:
	void* _data = nullptr;
	size_t _size = 0;

	// Implementation details (PIMPL or raw handles)
	// For simplicity of header-only, we might use void* and cast in cpp
	// but here we just store handles as void* or similar.
	// Linux: int fd;
	// Windows: HANDLE hFile, hMapping;
	struct Impl;
	Impl* _impl = nullptr;
};

/**
 * @brief Logic for array-like access over a mapped file.
 */
template <typename T>
class MappedArray {
public:
	MappedArray() = default;

	bool open(const std::string& filename, MappedFile::Mode mode, size_t count = 0) {
		return _file.open(filename, mode, count * sizeof(T));
	}

	// Direct access (unsafe if bounds unchecked, but fast)
	T* data() const { return static_cast<T*>(_file.data()); }
	T& operator[](size_t index) { return data()[index]; }
	const T& operator[](size_t index) const { return data()[index]; }

	size_t size() const { return _file.size() / sizeof(T); }
	size_t byteSize() const { return _file.size(); }

	bool resize(size_t count) {
		return _file.resize(count * sizeof(T));
	}
	bool grow(size_t count) {
		return _file.resize(count*sizeof(T) + _file.size());
	}

	void close() { _file.close(); }

	// STL-like helpers to improve compatibility with std::vector usage.
	T* begin() { return data(); }
	const T* begin() const { return data(); }
	T* end() { return data() + size(); }
	const T* end() const { return data() + size(); }

	bool empty() const { return size() == 0; }
	void clear() { resize(0); }

	// push_back and assign are implemented using resize and operator[].
	void push_back(const T& v) {
		const size_t s = size();
		resize(s + 1);
		(*this)[s] = v;
	}

	void reserve(size_t) { /* no-op for mapped files */ }

	void assign(size_t count, const T& value) {
		resize(count);
		for(size_t i = 0; i < count; ++i)
			(*this)[i] = value;
	}

	// Assign from a generic container (e.g., std::vector) with element conversion.
	template <typename Container>
	void assign_from(const Container& c) {
		resize(c.size());
		for (size_t i = 0; i < c.size(); ++i) {
			(*this)[i] = static_cast<T>(c[i]);
		}
	}

private:
	MappedFile _file;
};

} // namespace nx

#endif // NX_MAPPED_FILE_H

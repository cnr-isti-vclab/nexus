#ifndef NX_MAPPED_FILE_H
#define NX_MAPPED_FILE_H

#include <string>
#include <cstdint>
#include <vector>
#include <stdexcept>

namespace nx {

/**
 * @brief A simple wrapper around memory mapped files.
 * 
 * This class abstracts the OS specific mmap/CreateFileMapping calls.
 */

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

	/**
     * @brief Open or create a file and map it into memory.
     * @param filename Path to the file.
     * @param mode Open mode.
     * @param size Size in bytes. Required for READ_WRITE/TEMPORARY new files, or to resize.
     *             If 0 for READ_ONLY, uses existing file size.
	 */
	bool open(const std::string& filename, Mode mode, size_t size = 0);

	void close();

	// Resize the underlying file (and remap it).
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

	void close() { _file.close(); }

private:
	MappedFile _file;
};

} // namespace nx

#endif // NX_MAPPED_FILE_H

#include "mapped_file.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#endif

#include <cstdio>
#include <filesystem>

// We use std::filesystem for temp paths, require C++17
// #include <filesystem> or use a fallback if absolutely necessary.

namespace nx {

struct MappedFile::Impl {
#ifdef _WIN32
	HANDLE hFile = INVALID_HANDLE_VALUE;
	HANDLE hMapping = NULL;
#else
	int fd = -1;
	bool is_temp = false;
	std::string filename;
#endif
	Mode mode;
};

MappedFile::MappedFile() : _impl(new Impl) {}

MappedFile::~MappedFile() {
	close();
	delete _impl;
}

MappedFile::MappedFile(MappedFile&& other) noexcept
	: _data(other._data), _size(other._size), _impl(other._impl) {
	other._data = nullptr;
	other._size = 0;
	other._impl = nullptr;
}

MappedFile& MappedFile::operator=(MappedFile&& other) noexcept {
	if (this != &other) {
		close();
		delete _impl; // clean up current impl
		_data = other._data;
		_size = other._size;
		_impl = other._impl;

		other._data = nullptr;
		other._size = 0;
		other._impl = nullptr;
	}
	return *this;
}

bool MappedFile::open(const std::string& filename, Mode mode, size_t size) {
	if (isValid()) close();

	_impl->mode = mode;

#ifdef _WIN32
	// Windows Implementation
	DWORD access = (mode == READ_ONLY) ? GENERIC_READ : (GENERIC_READ | GENERIC_WRITE);
	DWORD share = FILE_SHARE_READ;
	DWORD creation = (mode == READ_ONLY) ? OPEN_EXISTING : OPEN_ALWAYS;
	if (mode == TEMPORARY) {
		creation = CREATE_ALWAYS;
		access = GENERIC_READ | GENERIC_WRITE;
		// FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE logic could be added
	}

	_impl->hFile = CreateFileA(filename.c_str(), access, share, NULL, creation, FILE_ATTRIBUTE_NORMAL, NULL);
	if (_impl->hFile == INVALID_HANDLE_VALUE) return false;

	if (mode != READ_ONLY && size > 0) {
		LARGE_INTEGER li;
		li.QuadPart = size;
		SetFilePointerEx(_impl->hFile, li, NULL, FILE_BEGIN);
		SetEndOfFile(_impl->hFile);
	} else if (size == 0) {
		LARGE_INTEGER li;
		GetFileSizeEx(_impl->hFile, &li);
		size = li.QuadPart;
	}
	_size = size;

	if (_size == 0) return true; // Empty file

	DWORD protect = (mode == READ_ONLY) ? PAGE_READONLY : PAGE_READWRITE;
	_impl->hMapping = CreateFileMappingA(_impl->hFile, NULL, protect, 0, 0, NULL);
	if (!_impl->hMapping) {
		CloseHandle(_impl->hFile);
		_impl->hFile = INVALID_HANDLE_VALUE;
		return false;
	}

	DWORD mapAccess = (mode == READ_ONLY) ? FILE_MAP_READ : FILE_MAP_WRITE;
	_data = MapViewOfFile(_impl->hMapping, mapAccess, 0, 0, 0);

	return (_data != nullptr);

#else
	// POSIX Implementation
	int flags = (mode == READ_ONLY) ? O_RDONLY : O_RDWR;
	if (mode != READ_ONLY) flags |= O_CREAT;

	// If temporary, complex logic, but let's stick to standard open
	if (mode == TEMPORARY) {
		// In robust code we might use mkstemp, but here we trust the filename or logic
		flags |= O_TRUNC;
		_impl->is_temp = true;
		_impl->filename = filename; // Remember to unlink
	}

	_impl->fd = ::open(filename.c_str(), flags, 0666);
	if (_impl->fd == -1) return false;

	struct stat sb;
	if (fstat(_impl->fd, &sb) == -1) {
		::close(_impl->fd);
		_impl->fd = -1;
		return false;
	}

	if (mode != READ_ONLY && size > 0) {
		if (ftruncate(_impl->fd, size) == -1) {
			::close(_impl->fd);
			_impl->fd = -1;
			return false;
		}
		_size = size;
	} else {
		_size = (size_t)sb.st_size;
	}

	if (_size == 0) {
		// mmap with length 0 is invalid.
		return true;
	}

	int prot = (mode == READ_ONLY) ? PROT_READ : (PROT_READ | PROT_WRITE);
	// MAP_SHARED is needed to write back to file
	// MAP_PRIVATE for copy-on-write
	void* ptr = mmap(NULL, _size, prot, MAP_SHARED, _impl->fd, 0);

	if (ptr == MAP_FAILED) {
		::close(_impl->fd);
		_impl->fd = -1;
		return false;
	}

	_data = ptr;
	return true;
#endif
}

void MappedFile::close() {
	if (_data) {
#ifdef _WIN32
		UnmapViewOfFile(_data);
#else
		munmap(_data, _size);
#endif
	}
	_data = nullptr;
	_size = 0;

	if (_impl) {
#ifdef _WIN32
		if (_impl->hMapping) CloseHandle(_impl->hMapping);
		if (_impl->hFile != INVALID_HANDLE_VALUE) CloseHandle(_impl->hFile);
		_impl->hMapping = NULL;
		_impl->hFile = INVALID_HANDLE_VALUE;
		// Deletion of temporary handled by user or added flags
#else
		if (_impl->fd != -1) {
			::close(_impl->fd);
			if (_impl->is_temp && !_impl->filename.empty()) {
				unlink(_impl->filename.c_str());
			}
		}
		_impl->fd = -1;
#endif
	}
}

bool MappedFile::resize(size_t new_size) {
	if (!_impl) return false;
	if (_impl->mode == READ_ONLY) return false;

	// Unmap
	if (_data) {
#ifdef _WIN32
		UnmapViewOfFile(_data);
		CloseHandle(_impl->hMapping);
		_impl->hMapping = NULL;
#else
		munmap(_data, _size);
#endif
		_data = nullptr;
	}

	// Resize file
	_size = new_size;
#ifdef _WIN32
	LARGE_INTEGER li;
	li.QuadPart = new_size;
	SetFilePointerEx(_impl->hFile, li, NULL, FILE_BEGIN);
	SetEndOfFile(_impl->hFile);
#else
	if (ftruncate(_impl->fd, new_size) == -1) return false;
#endif

	if (_size == 0) return true;

// Remap
#ifdef _WIN32
	_impl->hMapping = CreateFileMappingA(_impl->hFile, NULL, PAGE_READWRITE, 0, 0, NULL);
	if (!_impl->hMapping) return false;
	_data = MapViewOfFile(_impl->hMapping, FILE_MAP_WRITE, 0, 0, 0);
	return (_data != nullptr);
#else
	void* ptr = mmap(NULL, _size, PROT_READ | PROT_WRITE, MAP_SHARED, _impl->fd, 0);
	if (ptr == MAP_FAILED) return false;
	_data = ptr;
	return true;
#endif
}

} // namespace nx

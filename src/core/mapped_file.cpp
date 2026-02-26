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

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <iomanip>
#include <random>
#include <sstream>

// We use std::filesystem for temp paths, require C++17
// #include <filesystem> or use a fallback if absolutely necessary.

namespace nx {

namespace {

class TempFileHandle {
public:
#ifdef _WIN32
	HANDLE hFile = INVALID_HANDLE_VALUE;
	TempFileHandle(const std::string& filename) {
		std::string path = filename;
		if (path.empty()) {
			char temp_path[MAX_PATH + 1] = {};
			DWORD len = GetTempPathA(MAX_PATH, temp_path);
			assert(len > 0 && "GetTempPathA failed");
			if (len == 0) return;

			char temp_file[MAX_PATH + 1] = {};
			UINT ok = GetTempFileNameA(temp_path, "nxm", 0, temp_file);
			assert(ok != 0 && "GetTempFileNameA failed");
			if (ok == 0) return;

			path = temp_file;
		}

		DWORD access = GENERIC_READ | GENERIC_WRITE;
		DWORD share = FILE_SHARE_READ;
		DWORD creation = CREATE_ALWAYS;
		DWORD flags = FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE;

		hFile = CreateFileA(path.c_str(), access, share, NULL, creation, flags, NULL);
		assert(hFile != INVALID_HANDLE_VALUE && "CreateFileA failed for TEMPORARY");
	}
	~TempFileHandle() = default;
	bool valid() const { return hFile != INVALID_HANDLE_VALUE; }
#else
	int fd = -1;
	TempFileHandle(const std::string& filename) {
		std::string pattern = filename;
		if (pattern.empty()) {
			pattern = (std::filesystem::temp_directory_path() / "nx_mapped_XXXXXX").string();
		} else if (pattern.find("XXXXXX") == std::string::npos) {
			pattern += "XXXXXX";
		}

		std::vector<char> buf(pattern.begin(), pattern.end());
		buf.push_back('\0');

		fd = ::mkstemp(buf.data());
		assert(fd != -1 && "mkstemp failed for TEMPORARY");
		if (fd == -1) return;

		int rc = ::unlink(buf.data());
		assert(rc == 0 && "unlink failed for TEMPORARY");
	}
	~TempFileHandle() = default;
	bool valid() const { return fd != -1; }
#endif
};

} // namespace

struct MappedFile::Impl {
#ifdef _WIN32
	HANDLE hFile = INVALID_HANDLE_VALUE;
	HANDLE hMapping = NULL;
#else
	int fd = -1;
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

std::string MappedFile::makeTempPath(const std::string& folder, const std::string& prefix) {
	assert(!prefix.empty() && "prefix must not be empty");

	namespace fs = std::filesystem;
	fs::path base_path = folder.empty() ? fs::current_path() : fs::path(folder);

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<uint32_t> dis(0, 0xFFFFFFFFu);

	std::stringstream ss;
	ss << prefix << std::hex << std::setfill('0') << std::setw(8) << dis(gen);

	return (base_path / ss.str()).string();
}

bool MappedFile::open(const std::string& folder, const std::string& prefix, Mode mode, size_t size) {
	return open(makeTempPath(folder, prefix), mode, size);
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
		TempFileHandle temp(filename);
		assert(temp.valid() && "Temporary file creation failed");
		if (!temp.valid()) return false;
		_impl->hFile = temp.hFile;
	} else {
		_impl->hFile = CreateFileA(filename.c_str(), access, share, NULL, creation, FILE_ATTRIBUTE_NORMAL, NULL);
		assert(_impl->hFile != INVALID_HANDLE_VALUE && "CreateFileA failed");
		if (_impl->hFile == INVALID_HANDLE_VALUE) return false;
	}

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
	assert(_impl->hMapping && "CreateFileMappingA failed");
	if (!_impl->hMapping) {
		CloseHandle(_impl->hFile);
		_impl->hFile = INVALID_HANDLE_VALUE;
		return false;
	}

	DWORD mapAccess = (mode == READ_ONLY) ? FILE_MAP_READ : FILE_MAP_WRITE;
	_data = MapViewOfFile(_impl->hMapping, mapAccess, 0, 0, 0);
	assert(_data && "MapViewOfFile failed");

	return (_data != nullptr);

#else
	// POSIX Implementation
	int flags = (mode == READ_ONLY) ? O_RDONLY : O_RDWR;
	if (mode != READ_ONLY) flags |= O_CREAT;

	if (mode == TEMPORARY) {
		TempFileHandle temp(filename);
		assert(temp.valid() && "Temporary file creation failed");
		if (!temp.valid()) return false;
		_impl->fd = temp.fd;
	} else {
		_impl->fd = ::open(filename.c_str(), flags, 0666);
		assert(_impl->fd != -1 && "open failed");
		if (_impl->fd == -1) return false;
	}

	struct stat sb;
	if (fstat(_impl->fd, &sb) == -1) {
		assert(false && "fstat failed");
		::close(_impl->fd);
		_impl->fd = -1;
		return false;
	}

	if (mode != READ_ONLY && size > 0) {
		if (ftruncate(_impl->fd, size) == -1) {
			assert(false && "ftruncate failed");
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
		assert(false && "mmap failed");
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

#include "qtnexusfile.h"

namespace nx {
	void nx::QTNexusFile::setFileName(const char* uri)
	{
		file.setFileName(uri);
	}

	bool nx::QTNexusFile::open(OpenMode openmode)
	{
		QIODevice::OpenMode mode;
		if (openmode & OpenMode::Read) mode |= QIODevice::ReadOnly;
		if (openmode & OpenMode::Write) mode |= QIODevice::WriteOnly;
		if (openmode & OpenMode::Append) mode |= QIODevice::Append;

		return file.open(mode);
	}

	long long int nx::QTNexusFile::read(char* where, size_t length)
	{
		return file.read(where, length);
	}


	long long int nx::QTNexusFile::write(char* from, size_t length)
	{
		return file.write(from, length);
	}

	size_t QTNexusFile::size()
	{
		return file.size();
	}

	void* nx::QTNexusFile::map(size_t from, size_t size)
	{
		return file.map(from, size);
	}

	bool nx::QTNexusFile::unmap(void* mapped)
	{
		return file.unmap((uchar*)mapped);
	}

	bool nx::QTNexusFile::seek(size_t to)
	{
		return file.seek(to);
	}
}
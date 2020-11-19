#include "qtnexusfile.h"

namespace nx {
	NexusFile* NexusFile::create() { return new QTNexusFile(); }
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

	int nx::QTNexusFile::read(char* where, unsigned int length)
	{
		return file.read(where, length);
	}

	void* nx::QTNexusFile::map(unsigned int from, unsigned int size)
	{
		return file.map(from, size);
	}

	bool nx::QTNexusFile::unmap(void* mapped)
	{
		return file.unmap((uchar*)mapped);
	}

	bool nx::QTNexusFile::seek(unsigned int to)
	{
		return file.seek(to);
	}
}
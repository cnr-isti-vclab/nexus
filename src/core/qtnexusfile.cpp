#include "qtnexusfile.h"
#include <QFile>

namespace nx {
	void nx::QTNexusFile::setFileName(const char* uri)
	{
		file.setFileName(uri);
	}
	const char *nx::QTNexusFile::fileName() {
		QByteArray ba = file.fileName().toUtf8();
		return ba.constData();
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

	char *nx::QTNexusFile::loadDZNode(uint32_t n) {
		QString fname = file.fileName();
		// expect a companion folder named "<base>_files" like the saver produces
		if(fname.size() < 5)
			return nullptr;
		QString basename = fname.left(fname.size()-4) + "_files";
		QString nodefile = QString("%1/%2.nxn").arg(basename).arg(n);
		return readDZFile(nodefile);
	}

	void nx::QTNexusFile::dropDZNode(char *data) {
		delete [] data;
	}

	char *nx::QTNexusFile::loadDZTex(uint32_t n) {
		QString fname = file.fileName();
		if(fname.size() < 5)
			return nullptr;
		QString basename = fname.left(fname.size()-4) + "_files";
		QString texfile = QString("%1/%2.jpg").arg(basename).arg(n);
		return readDZFile(texfile);
	}

	void nx::QTNexusFile::dropDZTex(char *data) {
		delete [] data;
	}

	char *nx::QTNexusFile::readDZFile(const QString &path) {
		QFile f(path);
		if(!f.open(QIODevice::ReadOnly))
			return nullptr;
		QByteArray ba = f.readAll();
		char *buf = new char[ba.size()];
		if(ba.size())
			memcpy(buf, ba.constData(), ba.size());
		return buf;
	}

}

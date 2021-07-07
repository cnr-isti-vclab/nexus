#ifndef NX_QTNEXUSFILE_H
#define NX_QTNEXUSFILE_H

#include "nexusfile.h"
#include <QFile>

namespace nx {
	class QTNexusFile 
		: public NexusFile {
	private:
		QFile file; 
	public:
		void setFileName(const char* uri) override;
		bool open(OpenMode openmode) override;
		long long int read(char* where, size_t length) override;
		long long int write(char* from, size_t length) override;
		size_t size() override;
		void* map(size_t from, size_t size) override;
		bool unmap(void* mapped) override;
		bool seek(size_t to) override;
	};
}

#endif // NX_QTNEXUSFILE_H
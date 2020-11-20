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
		int read(char* where, unsigned int length) override;
		int write(char* from, unsigned int length) override;
		int size() override;
		void* map(unsigned int from, unsigned int size) override;
		bool unmap(void* mapped) override;
		bool seek(unsigned int to) override;
	};
}

#endif // NX_QTNEXUSFILE_H
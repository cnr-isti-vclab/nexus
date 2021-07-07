#ifndef NX_NEXUSFILE_H
#define NX_NEXUSFILE_H

#include <stddef.h>
namespace nx {
	class NexusFile {
	public:
		enum OpenMode {
			Read		= 1,
			Write		= 2,
			Append		= 4,
			ReadWrite	= Read | Write,
		};
		virtual ~NexusFile() {}
		virtual void setFileName(const char* uri) = 0;
		virtual bool open(OpenMode mode) = 0;
		virtual long long int read(char* where, size_t length) = 0;
		virtual long long int write(char* from, size_t length) = 0;
		virtual size_t size() = 0;
		virtual void* map(size_t from, size_t size) = 0;
		virtual bool unmap(void* mapped) = 0;
		virtual bool seek(size_t to) = 0;
	};
}

#endif // NX_NEXUSFILE_H

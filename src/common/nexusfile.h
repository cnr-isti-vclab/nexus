#ifndef NX_NEXUSFILE_H
#define NX_NEXUSFILE_H

#include <stddef.h>
#include <cstdint>

namespace nx {
	class NexusFile {
	public:
		enum OpenMode {
			Read		= 0b00000001,
			Write		= 0b00000010,
			Append		= 0b00000100,
			ReadWrite	= Read | Write,
		};
		virtual ~NexusFile() {}
		virtual void setFileName(const char* uri) = 0;
		virtual const char *fileName()  = 0;
		virtual bool open(OpenMode mode) = 0;
		virtual long long int read(char* where, size_t length) = 0;
		virtual long long int write(char* from, size_t length) = 0;
		virtual size_t size() = 0;
		virtual void* map(size_t from, size_t size) = 0;
		virtual bool unmap(void* mapped) = 0;
		virtual bool seek(size_t to) = 0;

		virtual char *loadDZNode(uint32_t n) = 0;
		virtual void dropDZNode(char *data) = 0;
		virtual char *loadDZTex(uint32_t n) = 0;
		virtual void dropDZTex(char *data) = 0;
	};
}

#endif // NX_NEXUSFILE_H

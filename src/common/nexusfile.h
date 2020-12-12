#ifndef NX_NEXUSFILE_H
#define NX_NEXUSFILE_H
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
		virtual bool open(OpenMode mode) = 0;
		virtual int read(char* where, unsigned int length) = 0;
		virtual int write(char* from, unsigned int length) = 0;
		virtual int size() = 0;
		virtual void* map(unsigned int from, unsigned int size) = 0;
		virtual bool unmap(void* mapped) = 0;
		virtual bool seek(unsigned int to) = 0;
	};
}

#endif // NX_NEXUSFILE_H

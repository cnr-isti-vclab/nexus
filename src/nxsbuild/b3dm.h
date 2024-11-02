#ifndef B3DM_H
#define B3DM_H

#include <stdint.h>
#include <ostream>

struct b3dmHeader {
	char magic[4] = {'b', '3', 'd', 'm'};
	uint32_t version = 1;
	uint32_t byteLength;
	uint32_t featureTableJSONByteLength = 0;
	uint32_t featureTableBinaryByteLength = 0;
	uint32_t batchTableJSONByteLength = 0;
	uint32_t batchTableBinaryByteLength = 0;

	inline uint32_t size() { return 28; }
};


class b3dm
{
	public:
	b3dm() = delete;
	b3dm(int binaryByteLength);
	void writeToStream(std::ostream &os);
	std::string getEndPadding() {return _endPadding;};

	private:
	b3dmHeader _header;
	std::string _featureTableHeader;
	std::string _padding = "";
	std::string _endPadding = "";

};
#endif // B3DM_H

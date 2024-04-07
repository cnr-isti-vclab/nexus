#include "b3dm.h"
#include "deps/json.hpp"

using namespace nlohmann;

b3dm::b3dm(int binaryByteLength) {

	json j;
	j["BATCH_LENGTH"] = 0;
	_featureTableHeader = j.dump();
	_header.featureTableJSONByteLength = std::streamsize(j.dump().size());

	int byteOffset = _header.size() + _header.featureTableJSONByteLength;
	int paddingSize = byteOffset % 8;
	if (paddingSize != 0) {
		paddingSize = 8 - paddingSize;
		_padding = std::string(size_t(paddingSize), ' ');
		_featureTableHeader.append(_padding);
		_header.featureTableJSONByteLength = std::streamsize(_featureTableHeader.size());
	}

	int endPaddingSize = (_header.size() + _header.featureTableJSONByteLength + binaryByteLength) % 8;
	if(endPaddingSize != 0) {
		endPaddingSize = 8 - endPaddingSize;
		_endPadding = std::string(size_t(endPaddingSize), ' ');
	}

	_header.byteLength = _header.size() + _header.featureTableJSONByteLength + binaryByteLength + endPaddingSize;
}

void b3dm::writeToStream(std::ostream &os) {

	os.write((char *) &_header.magic[0], sizeof(char) * 4);
	os.write((char *) &_header.version, sizeof(uint32_t));
	os.write((char *) &_header.byteLength, sizeof(uint32_t));
	os.write((char *) &_header.featureTableJSONByteLength, sizeof(uint32_t));
	os.write((char *) &_header.featureTableBinaryByteLength, sizeof(uint32_t));
	os.write((char *) &_header.batchTableJSONByteLength, sizeof(uint32_t));
	os.write((char *) &_header.batchTableBinaryByteLength, sizeof(uint32_t));
	os.write(_featureTableHeader.c_str(), _header.featureTableJSONByteLength);
}


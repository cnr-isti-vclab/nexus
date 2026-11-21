#ifndef JPEGDECODER_H_
#define JPEGDECODER_H_

#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <vector>

#include <jpeglib.h>

class JpegDecoder {
public:
	JpegDecoder();
	~JpegDecoder();

	JpegDecoder(const JpegDecoder&) = delete;
	void operator=(const JpegDecoder&) = delete;

	 //JCS_GRAYSCALE, JCS_RGB, JCS_YCbCr, or JCS_CMYK.

	J_COLOR_SPACE getColorSpace() const;
	void setColorSpace(J_COLOR_SPACE space);
	J_COLOR_SPACE getJpegColorSpace() const;

	bool decode(uint8_t* buffer, size_t len, uint8_t*& img, int& width, int& height);
	bool decode(const char* path, uint8_t*& img, int& width, int& height);
	bool decode(FILE* file, uint8_t*& img, int& width, int& height);

	//file streaming reading support
	bool init(const char* path, int &width, int &height);

	size_t rowSize() { return decInfo.image_width * decInfo.num_components; }

	//buffer must have rows*rowSize() space at least!
	size_t readRows(int rows, uint8_t *buffer); //return false on end.
	bool finish();
	bool restart();
	bool chromaSubsampled() { return subsampled; }

	// ICC color profile support
	bool hasICCProfile() const { return !icc_profile.empty(); }
	const std::vector<uint8_t>& getICCProfile() const { return icc_profile; }

private:
	FILE *file = nullptr;
	bool init(int &width, int &height);
	bool decode(uint8_t*& img, int& width, int& height);
	void extractICCProfile();

	jpeg_decompress_struct decInfo;
	jpeg_error_mgr errMgr;

	bool subsampled = false;
	std::vector<uint8_t> icc_profile;
};

#endif // JPEGDECODER_H_

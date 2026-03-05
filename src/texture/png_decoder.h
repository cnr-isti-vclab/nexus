#ifndef PNG_DECODER_H
#define PNG_DECODER_H

#include <cstdint>
#include <cstdio>

#include <png.h>

// Streaming PNG decoder with the same interface as JpegDecoder.
// Call init() once, then readRows() repeatedly.
class PngDecoder {
public:
	PngDecoder();
	~PngDecoder();

	PngDecoder(const PngDecoder&) = delete;
	void operator=(const PngDecoder&) = delete;

	// Open file, decode header, return true on success.
	// width and height are set to the image dimensions.
	// Output is always RGB (3 components, 8 bits/channel).
	bool init(const char* path, int& width, int& height);

	// Bytes per row: width * 3.
	size_t rowSize() const { return row_bytes_; }
	int width() const { return width_; }
	int height() const { return height_; }

	// Read up to `rows` rows into buffer (must have rows*rowSize() bytes).
	// Returns the number of rows actually read.
	size_t readRows(int rows, uint8_t* buffer);

private:
	FILE* file_ = nullptr;
	png_structp png_ptr_ = nullptr;
	png_infop info_ptr_ = nullptr;

	int width_ = 0;
	int height_ = 0;
	int rows_read_ = 0;
	size_t row_bytes_ = 0;
};

#endif // PNG_DECODER_H

#include "png_decoder.h"

#include <cassert>
#include <cstdlib>
#include <stdexcept>

PngDecoder::PngDecoder() = default;

PngDecoder::~PngDecoder() {
	if (png_ptr_)
		png_destroy_read_struct(&png_ptr_, info_ptr_ ? &info_ptr_ : nullptr, nullptr);
	if (file_)
		fclose(file_);
}

bool PngDecoder::init(const char* path, int& width, int& height) {
	file_ = fopen(path, "rb");
	if (!file_)
		return false;

	// Check signature
	uint8_t sig[8];
	if (fread(sig, 1, 8, file_) != 8 || !png_check_sig(sig, 8))
		return false;

	png_ptr_ = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
	assert(png_ptr_ && "png_create_read_struct failed");

	info_ptr_ = png_create_info_struct(png_ptr_);
	assert(info_ptr_ && "png_create_info_struct failed");

	if (setjmp(png_jmpbuf(png_ptr_)))
		throw std::runtime_error("libpng: error reading image header");

	png_init_io(png_ptr_, file_);
	png_set_sig_bytes(png_ptr_, 8);   // already consumed above
	png_read_info(png_ptr_, info_ptr_);

	// Normalize to 8-bit RGB:
	//   - expand palette / grayscale / tRNS to full depth
	//   - expand 16-bit depth to 8-bit
	//   - convert grayscale to RGB
	//   - strip alpha channel if present
	png_set_expand(png_ptr_);
	png_set_scale_16(png_ptr_);
	png_set_gray_to_rgb(png_ptr_);
	png_set_strip_alpha(png_ptr_);

	png_read_update_info(png_ptr_, info_ptr_);

	width_    = static_cast<int>(png_get_image_width(png_ptr_,  info_ptr_));
	height_   = static_cast<int>(png_get_image_height(png_ptr_, info_ptr_));
	row_bytes_ = png_get_rowbytes(png_ptr_, info_ptr_);

	assert(row_bytes_ == static_cast<size_t>(width_) * 3u &&
		"Unexpected row byte count after PNG transform setup");

	width  = width_;
	height = height_;
	return true;
}

size_t PngDecoder::readRows(int rows, uint8_t* buffer) {
	assert(png_ptr_ && info_ptr_);
	int remaining = height_ - rows_read_;
	int to_read   = (rows < remaining) ? rows : remaining;

	if (setjmp(png_jmpbuf(png_ptr_)))
		throw std::runtime_error("libpng: error reading scanline");

	for (int i = 0; i < to_read; i++) {
		png_read_row(png_ptr_, buffer + static_cast<size_t>(i) * row_bytes_, nullptr);
	}
	rows_read_ += to_read;
	return static_cast<size_t>(to_read);
}

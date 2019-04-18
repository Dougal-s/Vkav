#include <cstdio>
#include <stdexcept>
#include <utility>

#include <iostream>

#include <png.h>

#include "Image.hpp"

static unsigned char** emptyBuffer() {
	unsigned char** buffer = new unsigned char*[1];
	buffer[0] = new unsigned char[4]{0, 0, 0, 0};
	return buffer;
}

class PNG {
public:
	int init(const std::filesystem::path& filePath) {
		file = fopen(filePath.c_str(), "rb");
		if (!file) {
			std::cerr << "failed to open image!" << std::endl;
			return 1;
		}

		unsigned char sig[8];
		if (fread(reinterpret_cast<void*>(sig), 1, 8, file) != 8) {
			std::cerr << "failed to read png signature!" << std::endl;
			return 1;
		}

		if (!png_check_sig(sig, 8)) {
			std::cerr << "invalid png file!" << std::endl;
			return 1;
		}

		pPng = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
		if (!pPng) {
			std::cerr << "failed to create png struct!" << std::endl;
			return 4;
		}

		pInfo = png_create_info_struct(pPng);
		if (!pInfo) {
			std::cerr << "failed to create png info struct!" << std::endl;
			png_destroy_read_struct(&pPng, nullptr, nullptr);
			return 4;
		}

		if (setjmp(png_jmpbuf(pPng))) {
			png_destroy_read_struct(&pPng, &pInfo, nullptr);
			return 2;
		}

		png_init_io(pPng, file);
		png_set_sig_bytes(pPng, 8);
		png_read_info(pPng, pInfo);

		png_get_IHDR(pPng, pInfo, &imgWidth, &imgHeight, &bitDepth, &colorType, nullptr, nullptr, nullptr);

		return 0;
	}

	void readImage() {
		// if the image has a bit depth of 16, reduce it to a bit depth of 8
		if (bitDepth == 16) {
			png_set_strip_16(pPng);
		}

		if (colorType == PNG_COLOR_TYPE_PALETTE) {
			png_set_palette_to_rgb(pPng);
		}

		if (colorType == PNG_COLOR_TYPE_GRAY && bitDepth < 8) {
			png_set_expand_gray_1_2_4_to_8(pPng);
		}

		if (png_get_valid(pPng, pInfo, PNG_INFO_tRNS)) {
			png_set_tRNS_to_alpha(pPng);
		}

		// If the image is missing an alpha channel then fill it with 0xff

		if (
			colorType == PNG_COLOR_TYPE_RGB
			|| colorType == PNG_COLOR_TYPE_GRAY
			|| colorType == PNG_COLOR_TYPE_PALETTE
		) {
			png_set_filler(pPng, 0xff, PNG_FILLER_AFTER);
		}

		// if the image is grayscale then convert it to rgb
		if (
			colorType == PNG_COLOR_TYPE_GRAY
			|| colorType == PNG_COLOR_TYPE_GRAY_ALPHA
		) {
			png_set_gray_to_rgb(pPng);
		}

		// Apply transformations
		png_read_update_info(pPng, pInfo);

		// read from buffer
		image = new png_bytep[imgHeight];
		for (size_t y = 0; y < imgHeight; ++y) {
			image[y] = new png_byte[png_get_rowbytes(pPng, pInfo)];
		}

		png_read_image(pPng, image);

		fclose(file);

		png_destroy_read_struct(&pPng, &pInfo, nullptr);
	}

	unsigned char** getBuffer() {
		return reinterpret_cast<unsigned char**>(image);
	}

	size_t getHeight() {
		return imgHeight;
	}

	size_t getWidth() {
		return imgWidth;
	}

private:
	FILE* file;

	png_structp pPng;
	png_infop pInfo;

	int bitDepth, colorType;
	png_uint_32 imgWidth, imgHeight;

	png_bytep* image;
};

unsigned char** readPNG(const std::filesystem::path& filePath, size_t& width, size_t& height) {
	PNG png;
	int error;
	if ((error = png.init(filePath))) {
		if (error == 2) {
			std::cerr << "failed to read png image!" << std::endl;
		}
		width = 1;
		height = 1;
		return emptyBuffer();
	}
	png.readImage();
	width = png.getWidth();
	height = png.getHeight();
	return png.getBuffer();
}

unsigned char** readImg(const std::filesystem::path& filePath, size_t& width, size_t& height) {
	if (filePath.extension() == ".png") {
		return readPNG(filePath, width, height);
	}

	std::cerr << "unrecognized image type!" << std::endl;
	width = 1;
	height = 1;
	return emptyBuffer();
}

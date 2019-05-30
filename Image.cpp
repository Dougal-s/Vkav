#include <csetjmp>
#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>

// For png files
#ifndef DISABLE_PNG
#include <png.h>
#endif
// for jpeg files
#ifndef DISABLE_JPEG
#include <jpeglib.h>
#endif

#include "Image.hpp"

namespace {
	class Image {
	public:
		virtual void init(const std::filesystem::path& filepath) = 0;
		virtual void readImage() = 0;
		virtual unsigned char** getBuffer() = 0;
		virtual size_t getWidth() const = 0;
		virtual size_t getHeight() const = 0;
		virtual ~Image() = default;

		static unsigned char** emptyBuffer() {
			unsigned char** buffer = new unsigned char*[1];
			buffer[0] = new unsigned char[4]{0, 0, 0, 0};
			return buffer;
		}

		static Image* create(const std::filesystem::path& filePath);
		static void destroy(Image* image);
	};

#ifndef DISABLE_PNG
	class PNG : public Image {
	public:
		void init(const std::filesystem::path& filePath) override {
			file = fopen(filePath.c_str(), "rb");
			if (!file) throw std::runtime_error("failed to open image!");

			unsigned char sig[8];
			if (fread(reinterpret_cast<void*>(sig), 1, 8, file) != 8) {
				fclose(file);
				throw std::runtime_error("failed to read png signature!");
			}

			if (!png_check_sig(sig, 8))
				throw std::runtime_error("invalid png file!");

			pPng = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr,
			                              nullptr, nullptr);
			if (!pPng) throw std::runtime_error("failed to create png struct!");

			pInfo = png_create_info_struct(pPng);
			if (!pInfo) {
				png_destroy_read_struct(&pPng, nullptr, nullptr);
				throw std::runtime_error("failed to create png info struct!");
			}

			if (setjmp(png_jmpbuf(pPng))) {
				png_destroy_read_struct(&pPng, &pInfo, nullptr);
				throw std::runtime_error("failed to read PNG!");
			}

			png_init_io(pPng, file);
			png_set_sig_bytes(pPng, 8);
			png_read_info(pPng, pInfo);

			png_get_IHDR(pPng, pInfo, &imgWidth, &imgHeight, &bitDepth,
			             &colorType, nullptr, nullptr, nullptr);
		}

		void readImage() override {
			// if the image has a bit depth of 16, reduce it to a bit depth of 8
			if (bitDepth == 16) png_set_strip_16(pPng);

			if (colorType == PNG_COLOR_TYPE_PALETTE)
				png_set_palette_to_rgb(pPng);

			if (colorType == PNG_COLOR_TYPE_GRAY && bitDepth < 8)
				png_set_expand_gray_1_2_4_to_8(pPng);

			if (png_get_valid(pPng, pInfo, PNG_INFO_tRNS))
				png_set_tRNS_to_alpha(pPng);

			// If the image is missing an alpha channel then fill it with 0xff

			if (colorType == PNG_COLOR_TYPE_RGB ||
			    colorType == PNG_COLOR_TYPE_GRAY ||
			    colorType == PNG_COLOR_TYPE_PALETTE)
				png_set_filler(pPng, 0xff, PNG_FILLER_AFTER);

			// if the image is grayscale then convert it to rgb
			if (colorType == PNG_COLOR_TYPE_GRAY ||
			    colorType == PNG_COLOR_TYPE_GRAY_ALPHA)
				png_set_gray_to_rgb(pPng);

			// Apply transformations
			png_read_update_info(pPng, pInfo);

			// read from buffer
			image = new png_bytep[imgHeight];
			for (size_t y = 0; y < imgHeight; ++y)
				image[y] = new png_byte[png_get_rowbytes(pPng, pInfo)];

			png_read_image(pPng, image);

			fclose(file);

			png_destroy_read_struct(&pPng, &pInfo, nullptr);
		}

		unsigned char** getBuffer() override {
			return reinterpret_cast<unsigned char**>(image);
		}

		size_t getHeight() const override { return imgHeight; }

		size_t getWidth() const override { return imgWidth; }

		~PNG() override = default;

	private:
		FILE* file;

		png_structp pPng;
		png_infop pInfo;

		int bitDepth, colorType;
		png_uint_32 imgWidth, imgHeight;

		png_bytep* image;
	};
#endif

#ifndef DISABLE_JPEG
	class JPEG : public Image {
	public:
		void init(const std::filesystem::path& filePath) override {
			file = fopen(filePath.c_str(), "rb");
			if (!file) throw std::runtime_error("failed to open image!");

			cInfo.err = jpeg_std_error(&error.pub);
			error.pub.error_exit = errorExit;

			if (setjmp(error.setjmpBuffer)) {
				jpeg_destroy_decompress(&cInfo);
				fclose(file);
				throw std::runtime_error("failed to read JPEG!");
			}

			jpeg_create_decompress(&cInfo);

			jpeg_stdio_src(&cInfo, file);
		}

		void readImage() override {
			jpeg_read_header(&cInfo, TRUE);

			cInfo.out_color_space = JCS_EXT_RGBA;

			jpeg_start_decompress(&cInfo);

			imgWidth = cInfo.output_width;
			rowSize = cInfo.output_width * cInfo.output_components;
			imgHeight = cInfo.output_height;

			image = new unsigned char*[imgHeight];

			while (cInfo.output_scanline < cInfo.output_height) {
				image[cInfo.output_scanline] = new unsigned char[rowSize];
				jpeg_read_scanlines(&cInfo, image + cInfo.output_scanline, 1);
			}

			jpeg_finish_decompress(&cInfo);

			jpeg_destroy_decompress(&cInfo);
			fclose(file);
		}

		unsigned char** getBuffer() override { return image; }

		size_t getWidth() const override { return imgWidth; }

		size_t getHeight() const override { return imgHeight; }

		~JPEG() override = default;

	private:
		FILE* file;

		jpeg_decompress_struct cInfo;

		struct errorManager {
			jpeg_error_mgr pub;
			jmp_buf setjmpBuffer;
		} error;

		int rowSize;
		int imgWidth, imgHeight;

		unsigned char** image;

		[[noreturn]] static void errorExit(j_common_ptr cInfo) {
			errorManager* err = reinterpret_cast<errorManager*>(cInfo->err);
			(*cInfo->err->output_message)(cInfo);
			longjmp(err->setjmpBuffer, 1);
		}
	};
#endif

	Image* Image::create(const std::filesystem::path& filePath) {
#ifndef DISABLE_PNG
		if (filePath.extension() == ".png") return new PNG();
#endif

#ifndef DISABLE_JPEG
		if (filePath.extension() == ".jpg" || filePath.extension() == ".jpeg")
			return new JPEG();
#endif

		throw std::runtime_error("unrecognized image type!");
	}
}  // namespace

unsigned char** readImg(const std::filesystem::path& filePath, size_t& width,
                        size_t& height) {
	try {
		std::unique_ptr<Image> img(Image::create(filePath));
		unsigned char** buffer;

		img->init(filePath);
		img->readImage();
		width = img->getWidth();
		height = img->getHeight();
		buffer = img->getBuffer();
		return buffer;
	} catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		width = 1;
		height = 1;
		return Image::emptyBuffer();
	}
}

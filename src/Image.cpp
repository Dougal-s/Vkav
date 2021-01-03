#include <cstdio>
#include <fstream>
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

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define LOCATION __FILE__ ":" STR(__LINE__) ": "

class ImageFile::ImageImpl {
public:
	virtual unsigned char** data() = 0;
	virtual size_t width() const = 0;
	virtual size_t height() const = 0;
	virtual ~ImageImpl() = default;

	static ImageImpl* readImg(const std::filesystem::path& filePath);

private:
	static ImageImpl* open(const std::filesystem::path& filePath);
};

namespace {
	class BlankImage : public ImageFile::ImageImpl {
	public:
		BlankImage() {
			buffer = new unsigned char*[1];
			buffer[0] = new unsigned char[4]{0, 0, 0, 0};
		}

		unsigned char** data() override { return buffer; }

		size_t width() const override { return 1; }

		size_t height() const override { return 1; }

		~BlankImage() {
			delete[] buffer[0];
			delete[] buffer;
		}

	private:
		unsigned char** buffer;
	};

#ifndef DISABLE_PNG
	class PNG : public ImageFile::ImageImpl {
	public:
		PNG(const std::filesystem::path& filePath) {
			file = fopen(filePath.c_str(), "rb");
			if (!file) throw std::runtime_error(LOCATION "failed to open image!");

			unsigned char sig[8];
			if (fread(reinterpret_cast<void*>(sig), 1, 8, file) != 8) {
				fclose(file);
				throw std::runtime_error(LOCATION "failed to read png signature!");
			}

			if (!png_check_sig(sig, 8)) {
				fclose(file);
				throw std::runtime_error(LOCATION "invalid png file!");
			}

			pPng = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
			if (!pPng) {
				fclose(file);
				throw std::runtime_error(LOCATION "failed to create png struct!");
			}

			pInfo = png_create_info_struct(pPng);
			if (!pInfo) {
				fclose(file);
				throw std::runtime_error(LOCATION "failed to create png info struct!");
			}

			if (setjmp(png_jmpbuf(pPng))) {
				fclose(file);
				png_destroy_read_struct(&pPng, &pInfo, nullptr);
				throw std::runtime_error(LOCATION "failed to read PNG!");
			}

			png_init_io(pPng, file);
			png_set_sig_bytes(pPng, 8);
			png_read_info(pPng, pInfo);

			png_get_IHDR(pPng, pInfo, &imgWidth, &imgHeight, &bitDepth, &colorType, nullptr,
			             nullptr, nullptr);

			// if the image has a bit depth of 16, reduce it to a bit depth of 8
			if (bitDepth == 16) png_set_strip_16(pPng);

			if (colorType == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(pPng);

			if (colorType == PNG_COLOR_TYPE_GRAY && bitDepth < 8)
				png_set_expand_gray_1_2_4_to_8(pPng);

			if (png_get_valid(pPng, pInfo, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(pPng);

			// If the image is missing an alpha channel then fill it with 0xff
			if (colorType == PNG_COLOR_TYPE_RGB || colorType == PNG_COLOR_TYPE_GRAY ||
			    colorType == PNG_COLOR_TYPE_PALETTE)
				png_set_filler(pPng, 0xff, PNG_FILLER_AFTER);

			// if the image is grayscale then convert it to rgb
			if (colorType == PNG_COLOR_TYPE_GRAY || colorType == PNG_COLOR_TYPE_GRAY_ALPHA)
				png_set_gray_to_rgb(pPng);

			// Apply transformations
			png_read_update_info(pPng, pInfo);

			// read from buffer
			image = new png_bytep[imgHeight];
			for (size_t y = 0; y < imgHeight; ++y)
				image[y] = new png_byte[png_get_rowbytes(pPng, pInfo)];

			png_read_image(pPng, image);
			png_destroy_read_struct(&pPng, &pInfo, nullptr);
			fclose(file);
		}

		unsigned char** data() override { return reinterpret_cast<unsigned char**>(image); }

		size_t height() const override { return imgHeight; }

		size_t width() const override { return imgWidth; }

		~PNG() override {
			for (size_t y = 0; y < height(); ++y) delete[] image[y];
			delete[] image;
		}

	private:
		FILE* file;

		png_structp pPng;
		png_infop pInfo;

		int bitDepth, colorType;
		png_uint_32 imgWidth, imgHeight;

		png_bytep* image = nullptr;
	};
#endif

#ifndef DISABLE_JPEG
	class JPEG : public ImageFile::ImageImpl {
	public:
		JPEG(const std::filesystem::path& filePath) {
			file = fopen(filePath.c_str(), "rb");
			if (!file) throw std::runtime_error(LOCATION "failed to open image!");

			cInfo.err = jpeg_std_error(&error.pub);
			error.pub.error_exit = errorExit;
			error.parent = this;

			jpeg_create_decompress(&cInfo);

			jpeg_stdio_src(&cInfo, file);
			jpeg_read_header(&cInfo, TRUE);

#ifdef JCS_ALPHA_EXTENSIONS
			cInfo.out_color_space = JCS_EXT_RGBA;
#else
			cInfo.out_color_space = JCS_RGB;
#endif

			jpeg_start_decompress(&cInfo);

			imgWidth = cInfo.output_width;
			rowSize = cInfo.output_width * 4;
			imgHeight = cInfo.output_height;

			image = new unsigned char*[imgHeight]();

			while (cInfo.output_scanline < cInfo.output_height) {
				image[cInfo.output_scanline] = new unsigned char[rowSize];
				jpeg_read_scanlines(&cInfo, image + cInfo.output_scanline, 1);
#ifndef JCS_ALPHA_EXTENSIONS
				for (int pixel = cInfo.output_width - 1; pixel >= 0; --pixel) {
					image[cInfo.output_scanline - 1][4 * pixel] =
					    image[cInfo.output_scanline - 1][3 * pixel];
					image[cInfo.output_scanline - 1][4 * pixel + 1] =
					    image[cInfo.output_scanline - 1][3 * pixel + 1];
					image[cInfo.output_scanline - 1][4 * pixel + 2] =
					    image[cInfo.output_scanline - 1][3 * pixel + 2];
					image[cInfo.output_scanline - 1][4 * pixel + 3] =
					    image[cInfo.output_scanline - 1][3 * pixel + 3];
				}
#endif
			}

			jpeg_finish_decompress(&cInfo);
			jpeg_destroy_decompress(&cInfo);
			fclose(file);
		}

		unsigned char** data() override { return image; }

		size_t width() const override { return imgWidth; }

		size_t height() const override { return imgHeight; }

		~JPEG() override {
			for (size_t y = 0; y < height(); ++y) delete[] image[y];
			delete[] image;
		}

	private:
		FILE* file;

		jpeg_decompress_struct cInfo;

		struct errorManager {
			jpeg_error_mgr pub;
			JPEG* parent;
		} error;

		int rowSize;
		int imgWidth, imgHeight = 0;

		unsigned char** image;

		[[noreturn]] static void errorExit(j_common_ptr cInfo) {
			errorManager* err = reinterpret_cast<errorManager*>(cInfo->err);
			// cleanup
			jpeg_destroy_decompress(&err->parent->cInfo);
			fclose(err->parent->file);
			// deallocate image data
			for (size_t y = 0; y < err->parent->height(); ++y)
				if (err->parent->image[y]) delete[] err->parent->image[y];
			if (err->parent->height() > 0) delete[] err->parent->image;
			// get error message
			char err_msg[JMSG_LENGTH_MAX];
			(*cInfo->err->format_message)(cInfo, err_msg);
			// throw error
			throw std::runtime_error(std::string(LOCATION) + "failed to read JPEG!: " + err_msg);
		}
	};
#endif

	class BMP : public ImageFile::ImageImpl {
	public:
		BMP(const std::filesystem::path& filePath) {
			std::ifstream file(filePath, std::ios::binary | std::ios::in);
			file.read(reinterpret_cast<char*>(&header), sizeof(header));
			header.check();
			file.seekg(header.dataOffset, std::ios::beg);

			size_t rowSize = ((header.width * header.bitsPerPixel + 31) >> 5) << 2;

			image = new unsigned char*[height()];
			char* buffer = new char[rowSize];
			for (int y = height() - 1; y != -1; --y) {
				image[y] = new unsigned char[4 * width()];
				file.read(buffer, rowSize);
				for (size_t x = 0; x < width(); ++x) {
					switch (header.bitsPerPixel) {
						case 24:
							image[y][4 * x + 0] = buffer[3 * x + 2];
							image[y][4 * x + 1] = buffer[3 * x + 1];
							image[y][4 * x + 2] = buffer[3 * x + 0];
							image[y][4 * x + 3] = 0xff;
							break;
						case 32:
							image[y][4 * x + 0] = buffer[4 * x + 3];
							image[y][4 * x + 1] = buffer[4 * x + 2];
							image[y][4 * x + 2] = buffer[4 * x + 1];
							image[y][4 * x + 3] = buffer[4 * x + 0];
							break;
					}
				}
			}
			delete[] buffer;

			file.close();
		}

		unsigned char** data() override { return image; }

		size_t width() const override { return header.width; }

		size_t height() const override { return header.height; }

		~BMP() override {
			for (size_t y = 0; y < height(); ++y) delete[] image[y];
			delete[] image;
		}

	private:
		struct Header {
			// header
			uint8_t signature[2];  // 0
			uint32_t fileSize;     // 2
			uint8_t reserved[4];   // 6  // ignored
			uint32_t dataOffset;   // 10
			// info header
			uint32_t size;             // 14
			int32_t width;             // 18
			int32_t height;            // 22
			uint16_t planes;           // 26
			uint16_t bitsPerPixel;     // 28
			uint32_t compression;      // 30
			uint32_t imageSize;        // 34 // ignored
			int32_t xPixelsPerM;       // 38 // ignored
			int32_t yPixelsPerM;       // 42 // ignored
			uint32_t colorsUsed;       // 46 // ignored
			uint32_t importantColors;  // 50 // ignored

			void check() {
				if (signature[0] != 'B' || signature[1] != 'M')
					throw std::runtime_error(LOCATION "Invalid BMP file!");
				if (planes != 1)
					throw std::runtime_error(LOCATION "Invalid plane count in BMP file!");
				if (bitsPerPixel != 32 && bitsPerPixel != 24)
					throw std::runtime_error(LOCATION "Unsupported bitPerPixel value in BMP file!");
				if (compression != 0)
					throw std::runtime_error(LOCATION
					                         "Compressed BMP file are unsupported as of now!");
			}
		} __attribute__((packed)) header;

		unsigned char** image;
	};
}  // namespace

ImageFile::ImageImpl* ImageFile::ImageImpl::open(const std::filesystem::path& filePath) {
#ifndef DISABLE_PNG
	if (filePath.extension() == ".png") return new PNG(filePath);
#endif

#ifndef DISABLE_JPEG
	if (filePath.extension() == ".jpg" || filePath.extension() == ".jpeg")
		return new JPEG(filePath);
#endif

	if (filePath.extension() == ".bmp") return new BMP(filePath);

	throw std::runtime_error(LOCATION "unrecognized image type!");
}

ImageFile::ImageImpl* ImageFile::ImageImpl::readImg(const std::filesystem::path& filePath) {
	try {
		return open(filePath);
	} catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return new BlankImage();
	}
}

// Image File member functions

ImageFile::ImageFile() : impl(new BlankImage()) {}
ImageFile::ImageFile(const std::filesystem::path& filePath) { this->open(filePath); }

ImageFile::~ImageFile() {}

ImageFile& ImageFile::operator=(ImageFile&& other) {
	impl = std::move(other.impl);
	return *this;
}

void ImageFile::open(const std::filesystem::path& filePath) {
	impl.reset(ImageImpl::readImg(filePath));
}

unsigned char** ImageFile::data() { return impl->data(); }
unsigned char* ImageFile::operator[](size_t row) { return impl->data()[row]; }

size_t ImageFile::width() const { return impl->width(); }
size_t ImageFile::height() const { return impl->height(); }
size_t ImageFile::size() const { return impl->width() * impl->height() * 4; }

#pragma once
#ifndef IMAGE_HPP
#define IMAGE_HPP

#include <filesystem>
#include <memory>

class ImageFile {
public:
	ImageFile();
	ImageFile(const std::filesystem::path& filePath);
	~ImageFile();

	ImageFile& operator=(ImageFile&& other);

	void open(const std::filesystem::path& filePath);

	unsigned char** data();
	unsigned char* operator[](size_t row);

	size_t width() const;
	size_t height() const;
	size_t size() const;

	class ImageImpl;
private:
	std::unique_ptr<ImageImpl> impl;
};

#endif

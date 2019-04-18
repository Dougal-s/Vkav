#pragma once
#ifndef IMAGE_HPP
#define IMAGE_HPP

#include <filesystem>

unsigned char** readImg(const std::filesystem::path& filePath, size_t& imgWidth, size_t& imgHeight);

#endif

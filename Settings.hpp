#pragma once
#ifndef SETTINGS_HPP
#define SETTINGS_HPP

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

std::unordered_map<std::string, std::string> readConfigFile(const std::filesystem::path& filePath);
std::unordered_map<char, std::string> readCmdLineArgs(int argc, const char* argv[]);
std::vector<std::filesystem::path> getConfigLocations();

#endif

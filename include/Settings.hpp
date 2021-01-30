#pragma once
#ifndef SETTINGS_HPP
#define SETTINGS_HPP

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

std::string_view parseAsString(std::string_view string);
std::vector<std::string_view> parseAsArray(std::string_view string);
std::pair<std::string_view, std::string_view> parseAsPair(std::string_view string);

std::unordered_map<std::string, std::string> readConfigFile(const std::filesystem::path& filePath);
std::unordered_map<std::string, std::string> readCmdLineArgs(int argc, const char** argv);
std::vector<std::filesystem::path> getConfigLocations();
std::unordered_map<std::string, std::vector<std::filesystem::path>> getModules();
void installConfig();

#endif

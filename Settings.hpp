#pragma once
#ifndef SETTINGS_HPP
#define SETTINGS_HPP

#include <unordered_map>
#include <string>

std::unordered_map<std::string, std::string> readConfigFile(const std::string& filePath);
std::unordered_map<char, const char*> readCmdLineArgs(int argc, char* argv[]);

#endif

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#ifdef LINUX
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#elif defined(MACOS)
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#elif defined(WINDOWS)
#include <Combaseapi.h>
#include <Shlobj_api.h>
#else
#endif

#include "Settings.hpp"

#define S1(x) #x
#define S2(x) S1(x)
#define LOCATION __FILE__ ":" S2(__LINE__) ": "

std::unordered_map<std::string, std::string> readConfigFile(const std::filesystem::path& filePath) {
	std::unordered_map<std::string, std::string> variables;

	std::ifstream configFile(filePath);
	if (!configFile.is_open())
		throw std::runtime_error(LOCATION "Failed to open configuration file!");

	std::string line;
	std::string varName;
	std::string varVal;

	while (std::getline(configFile, line).good()) {
		// Check for comments
		if (const auto& position = line.find("//"); position != std::string::npos)
			line.erase(position);

		std::istringstream isLine(line);

		if (!std::getline(isLine, varName, '=')) continue;
		if (!std::getline(isLine, varVal)) continue;

		// Remove whitespaces
		varName.resize(std::remove_if(varName.begin(), varName.end(), isspace) - varName.begin());

		if (const auto& position = varVal.find('{');
		    position != std::string::npos)  // Check if the input is a list
			varVal = varVal.substr(position, varVal.find('}', position + 1) - position + 1);
		else if (const auto& position = varVal.find('\"');
		         position != std::string::npos)  // Check if the input is a string
			varVal = varVal.substr(position + 1, varVal.find('\"', position + 1) - position - 1);
		else  // remove whitespaces
			varVal.resize(std::remove_if(varVal.begin(), varVal.end(), isspace) - varVal.begin());

		variables.insert(std::make_pair(varName, varVal));
	}

	configFile.close();

	return variables;
}

std::unordered_map<std::string, std::string> readCmdLineArgs(int argc, const char* argv[]) {
	std::unordered_map<std::string, std::string> arguments(argc - 1);
	for (int i = 1; i < argc; ++i) {
		if (argv[i][0] != '-')
			throw std::invalid_argument(LOCATION ": Invalid command line argument!");

		std::string argValue;
		std::string argName = argv[i];

		for (size_t size = 0; argv[i][size]; ++size) {
			if (argv[i][size] == '=') {
				argName.resize(size);
				argValue = argv[i] + size + 1;
				break;
			}
		}

		if (argv[i][1] == '-') {
			static const std::unordered_set<std::string> validCmdLineFlags = {
			    "--verbose",   "--sink-name", "--device",  "--config",
			    "--amplitude", "--help",      "--version", "--install-config"};

			if (validCmdLineFlags.find(argName) == validCmdLineFlags.end())
				throw std::invalid_argument(LOCATION ": Invalid command line argument!");

			arguments.insert(std::make_pair(argv[i] + 2, argValue));

		} else {
			std::string key;
			static const std::unordered_map<char, std::string> cmdLineArgKeyMap = {
			    {'v', "verbose"},   {'s', "sink-name"}, {'d', "device"}, {'c', "config"},
			    {'a', "amplitude"}, {'h', "help"},      {'V', "version"}};
			if (const auto it = cmdLineArgKeyMap.find(argv[i][1]); it != cmdLineArgKeyMap.end())
				key = it->second;
			else
				throw std::invalid_argument(LOCATION ": Invalid command line argument!");

			const char* value;

			if (!argv[i][2] && i + 1 < argc && argv[i + 1][0] != '-')
				value = argv[++i];
			else
				value = argv[i] + 2;

			arguments.insert(std::make_pair(key, value));
		}
	}
	return arguments;
}

std::vector<std::filesystem::path> getConfigLocations() {
	std::vector<std::filesystem::path> configLocations;
#ifdef LINUX
	configLocations.resize(2);
	configLocations[0] = std::getenv("HOME");
	if (configLocations[0].empty()) configLocations[0] = getpwuid(geteuid())->pw_dir;
	configLocations[0] /= ".config/Vkav";
	configLocations[1] = "/etc/Vkav";
#elif defined(MACOS)
	configLocations.resize(2);
	configLocations[0] = std::getenv("HOME");
	if (configLocations[0].empty()) configLocations[0] = getpwuid(geteuid())->pw_dir;
	configLocations[0] /= "Library/Preferences";
	configLocations[1] = "/Library/Preferences";
#elif defined(WINDOWS)
	configLocations.resize(2);
	const char* path;
	SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_DEFAULT, NULL, path);
	configLocations[0] = path;
	configLocations[0] /= "Vkav";
	CoTaskMemFree(path);

	SHGetKnownFolderPath(FOLDERID_ProgramData, KF_FLAG_DEFAULT, NULL, path);
	configLocations[1] = path;
	configLocations[1] /= "Vkav";
	CoTaskMemFree(path);
#else
#endif

	for (auto it = configLocations.begin(); it != configLocations.end();) {
		if (!std::filesystem::is_directory(*it))
			it = configLocations.erase(it);
		else
			++it;
	}

	return configLocations;
}

void installConfig() {
	std::filesystem::path src, dst;
#ifdef LINUX
	src = "/etc/Vkav";
	dst = std::getenv("HOME");
	if (dst.empty()) dst = getpwuid(geteuid())->pw_dir;
	dst /= ".config/Vkav";
#elif defined(MACOS)
	src = "/Library/Preferences";
	dst = std::getenv("HOME");
	if (dst.empty()) dst = getpwuid(geteuid())->pw_dir;
	dst /= "Library/Preferences";
#elif defined(WINDOWS)
	const char* path;
	SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_DEFAULT, NULL, path);
	dst = path;
	dst /= "Vkav";
	CoTaskMemFree(path);

	SHGetKnownFolderPath(FOLDERID_ProgramData, KF_FLAG_DEFAULT, NULL, path);
	src = path;
	src /= "Vkav";
	CoTaskMemFree(path);
#else
	throw std::runtime_error(LOCATION "Unsupported operating system!");
#endif

	if (!std::filesystem::exists(src))
		throw std::runtime_error(LOCATION "Source directory: " + std::string(src) +
		                         " does not exist!");

	std::cout << "Copying config files from " << src << " to " << dst << std::endl;
	std::filesystem::copy(src, dst,
	                      std::filesystem::copy_options::overwrite_existing |
	                          std::filesystem::copy_options::recursive);
}

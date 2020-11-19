#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
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

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define LOCATION __FILE__ ":" STR(__LINE__) ": "

std::string_view parseAsString(std::string_view string) {
	if ((string.front() == '"') ^ (string.back() == '"'))
		throw std::invalid_argument(LOCATION "Mismatched double quotes!");
	if (string.front() == '"') string = string.substr(1, string.size() - 2);
	return string;
}

std::vector<std::string_view> parseAsArray(std::string_view string) {
	if (string.front() == '{' && string.back() == '}') {
		std::vector<std::string_view> array;
		string = string.substr(1, string.size() - 2);
		while (true) {
			while (std::isspace(string.front())) string.remove_prefix(1);
			auto index = string.find(',');
			auto elem = string.substr(0, index);
			while (std::isspace(elem.back())) elem.remove_suffix(1);
			array.push_back(elem);
			if (index == std::string_view::npos)
				break;
			else
				string.remove_prefix(index + 1);
		}
		return array;
	} else {
		return std::vector<std::string_view>{string};
	}
}

std::pair<std::string_view, std::string_view> parseAsPair(std::string_view string) {
	if (!(string.front() == '{' && string.back() == '}'))
		throw std::invalid_argument(LOCATION "mismatched or missing curly brackets!");

	string = string.substr(1, string.size() - 2);
	while (std::isspace(string.front())) string.remove_prefix(1);
	while (std::isspace(string.back())) string.remove_suffix(1);

	auto index = string.find(',');
	if (index == std::string_view::npos) throw std::invalid_argument(LOCATION "expected ','!");

	std::pair pair{string.substr(0, index), string.substr(index + 1)};
	while (std::isspace(pair.second.front())) pair.second.remove_prefix(1);
	while (std::isspace(pair.first.back())) pair.first.remove_suffix(1);

	return pair;
}

std::unordered_map<std::string, std::string> readConfigFile(const std::filesystem::path& filePath) {
	std::unordered_map<std::string, std::string> variables;

	std::ifstream configFile(filePath);
	if (!configFile.is_open())
		throw std::runtime_error(LOCATION "Failed to open configuration file!");

	std::string line;

	while (std::getline(configFile, line).good()) {
		// Check for comments
		if (const auto& position = line.find("//"); position != std::string::npos)
			line.erase(position);

		auto index = line.find('=');
		if (index == std::string::npos) continue;

		std::string_view varName(line);
		varName.remove_suffix(line.size() - index);
		std::string_view varVal(line);
		varVal.remove_prefix(index + 1);

		while (std::isspace(varName.front())) varName.remove_prefix(1);
		while (std::isspace(varName.back())) varName.remove_suffix(1);

		while (std::isspace(varVal.front())) varVal.remove_prefix(1);
		while (std::isspace(varVal.back())) varVal.remove_suffix(1);

		variables.insert(std::make_pair(varName, varVal));
	}

	return variables;
}

std::unordered_map<std::string, std::string> readCmdLineArgs(int argc, const char** argv) {
	std::unordered_map<std::string, std::string> arguments(argc - 1);
	for (int i = 1; i < argc; ++i) {
		if (argv[i][0] != '-')
			throw std::invalid_argument(LOCATION ": Invalid command line argument!");

		std::string_view argName(argv[i]);
		std::string_view argValue;
		if (auto split = argName.find('='); split != std::string_view::npos) {
			argValue = argName.substr(split + 1);
			argName = argName.substr(0, split);
		}

		if (argValue.empty() && i + 1 < argc && argv[i + 1][0] != '-') argValue = argv[++i];

		if (argName.length() == 2) {
			static const std::unordered_map<char, std::string> cmdLineArgKeyMap = {
			    {'v', "--verbose"}, {'s', "--sinkName"},  {'d', "--physicalDevice"},
			    {'c', "--config"},  {'a', "--amplitude"}, {'h', "--help"},
			    {'V', "--version"}};
			if (auto key = cmdLineArgKeyMap.find(argName[1]); key != cmdLineArgKeyMap.end())
				argName = key->second;
			else
				throw std::invalid_argument(LOCATION ": Unrecognized command line argument!");
		}

		argName.remove_prefix(2);
		arguments.insert(std::make_pair(argName, argValue));
	}
	return arguments;
}

std::vector<std::filesystem::path> getConfigLocations() {
	std::vector<std::filesystem::path> configLocations;
#ifdef LINUX
	configLocations.resize(2);
	configLocations[0] = std::getenv("HOME");
	if (configLocations[0].empty()) configLocations[0] = getpwuid(geteuid())->pw_dir;
	configLocations[0] /= ".config/vkav";
	configLocations[1] = "/etc/vkav";
#elif defined(MACOS)
	configLocations.resize(2);
	configLocations[0] = std::getenv("HOME");
	if (configLocations[0].empty()) configLocations[0] = getpwuid(geteuid())->pw_dir;
	configLocations[0] /= "Library/Preferences/vkav";
	configLocations[1] = "../Resources/vkav";
#elif defined(WINDOWS)
	configLocations.resize(2);
	const char* path;
	SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_DEFAULT, NULL, path);
	configLocations[0] = path;
	configLocations[0] /= "vkav";
	CoTaskMemFree(path);

	SHGetKnownFolderPath(FOLDERID_ProgramData, KF_FLAG_DEFAULT, NULL, path);
	configLocations[1] = path;
	configLocations[1] /= "vkav";
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

std::unordered_map<std::string, std::vector<std::filesystem::path>> getModules() {
	auto configLocations = getConfigLocations();
	std::unordered_map<std::string, std::vector<std::filesystem::path>> modules;
	for (auto& configLocation : configLocations) {
		for (auto& module : std::filesystem::directory_iterator(configLocation / "modules")) {
			if (std::filesystem::exists(module.path() / "config") &&
			    std::filesystem::exists(module.path() / "1"))
				modules[module.path().filename()].push_back(module.path());
		}
	}
	return modules;
}

void installConfig() {
	std::filesystem::path src, dst;
#ifdef LINUX
	src = "/etc/vkav";
	dst = std::getenv("HOME");
	if (dst.empty()) dst = getpwuid(geteuid())->pw_dir;
	dst /= ".config/vkav";
#elif defined(MACOS)
	src = "../Resources/vkav";
	dst = std::getenv("HOME");
	if (dst.empty()) dst = getpwuid(geteuid())->pw_dir;
	dst /= "Library/Preferences/vkav";
#elif defined(WINDOWS)
	const char* path;
	SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_DEFAULT, NULL, path);
	dst = path;
	dst /= "vkav";
	CoTaskMemFree(path);

	SHGetKnownFolderPath(FOLDERID_ProgramData, KF_FLAG_DEFAULT, NULL, path);
	src = path;
	src /= "vkav";
	CoTaskMemFree(path);
#else
	throw std::runtime_error(LOCATION "Unsupported operating system!");
#endif

	if (!std::filesystem::exists(src))
		throw std::runtime_error(LOCATION "Source directory: " + std::string(src) +
		                         " does not exist!");

	std::filesystem::copy(src, dst,
	                      std::filesystem::copy_options::overwrite_existing |
	                          std::filesystem::copy_options::recursive);
	std::cout << "Copied config files from " << src << " to " << dst << std::endl;
}

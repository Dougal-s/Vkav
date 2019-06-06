#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "Settings.hpp"

std::unordered_map<std::string, std::string> readConfigFile(
    const std::filesystem::path& filePath) {
	std::unordered_map<std::string, std::string> variables;

	std::ifstream configFile(filePath);
	if (!configFile.is_open())
		throw std::runtime_error(__FILE__
		                         ": Failed to open configuration file!");

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
		varName.resize(std::remove_if(varName.begin(), varName.end(), isspace) -
		               varName.begin());

		if (const auto& position = varVal.find('{');
		    position != std::string::npos)  // Check if the input is a list
			varVal = varVal.substr(
			    position, varVal.find('}', position + 1) - position + 1);
		else if (const auto& position = varVal.find('\"');
		         position !=
		         std::string::npos)  // Check if the input is a string
			varVal = varVal.substr(
			    position + 1, varVal.find('\"', position + 1) - position - 1);
		else  // remove whitespaces
			varVal.resize(
			    std::remove_if(varVal.begin(), varVal.end(), isspace) -
			    varVal.begin());

		variables.insert(std::make_pair(varName, varVal));
	}

	configFile.close();

	return variables;
}

std::unordered_map<char, std::string> readCmdLineArgs(int argc,
                                                      const char* argv[]) {
	std::unordered_map<char, std::string> arguments(argc - 1);
	for (int i = 1; i < argc; ++i) {
		if (argv[i][0] != '-')
			throw std::invalid_argument(__FILE__
			                            ": Invalid command line argument!");

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
			char charKey = 0;

			static const std::unordered_map<std::string, char>
			    cmdLineArgKeyMap = {{"--verbose", 'v'},   {"--sink-name", 's'},
			                        {"--device", 'd'},    {"--config", 'c'},
			                        {"--amplitude", 'a'}, {"--help", 'h'},
			                        {"--version", 'V'}};

			if (const auto it = cmdLineArgKeyMap.find(argName);
			    it != cmdLineArgKeyMap.end()) {
				charKey = it->second;
			}

			if (charKey == 0)
				throw std::invalid_argument(__FILE__
				                            ": Invalid command line argument!");

			arguments.insert(std::make_pair(charKey, argValue));

		} else {
			static const std::unordered_set<char> validCmdLineFlags = {
			    'v', 's', 'd', 'c', 'a', 'h', 'V'};
			if (validCmdLineFlags.find(argv[i][1]) == validCmdLineFlags.end())
				throw std::invalid_argument(__FILE__
				                            ": Invalid command line argument!");

			char key = argv[i][1];
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

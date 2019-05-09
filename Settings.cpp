#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
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
		std::string::size_type position;
		// Check for comments
		if (position = line.find("//"); position != std::string::npos)
			line.erase(position);

		std::istringstream isLine(line);

		if (!std::getline(isLine, varName, '=')) continue;
		if (!std::getline(isLine, varVal)) continue;

		// Remove whitespaces
		varName.resize(std::remove_if(varName.begin(), varName.end(), isspace) -
		               varName.begin());

		if (position = varVal.find('{');
		    position != std::string::npos)  // Check if the input is a list
			varVal = varVal.substr(
			    position, varVal.find('}', position + 1) - position + 1);
		else if (position = varVal.find('\"');
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
		std::string argValue;
		std::string argName;
		for (size_t size = 0; argv[i][size] != 0; ++size) {
			if (argv[i][size] == '=') argValue = argv[i] + size + 1;
			if (argValue.empty()) argName.push_back(argv[i][size]);
		}

		if (argv[i][0] == '-') {
			if (argv[i][1] == '-') {
				char charKey = 0;

				if (argName == "--verbose") {
					charKey = 'v';
				} else if (argName == "--sink-name") {
					charKey = 's';
				} else if (argName == "--device") {
					charKey = 'd';
				} else if (argName == "--config-file") {
					charKey = 'c';
				} else if (argName == "--amplitude") {
					charKey = 'a';
				} else if (argName == "--help") {
					charKey = 'h';
				} else if (argName == "--version") {
					charKey = 'V';
				}

				if (charKey == 0) {
					throw std::invalid_argument(
					    __FILE__ ": Invalid command line argument!");
				}

				arguments.insert(std::make_pair(charKey, argValue));

			} else {
				if (argv[i][1] != 'v' && argv[i][1] != 's' &&
				    argv[i][1] != 'd' && argv[i][1] != 'c' &&
				    argv[i][1] != 'a' && argv[i][1] != 'h' &&
				    argv[i][1] != 'V') {
					throw std::invalid_argument(
					    __FILE__ ": Invalid command line argument!");
				}

				char key = argv[i][1];
				const char* value;
				if (argv[i][2] == '\0' && i + 1 < argc &&
				    argv[i + 1][0] != '-') {
					value = argv[i + 1];
					++i;
				} else {
					value = argv[i] + 2;
				}

				arguments.insert(std::make_pair(key, value));
			}
		} else {
			throw std::invalid_argument(__FILE__
			                            ": Invalid command line argument!");
		}
	}
	return arguments;
}

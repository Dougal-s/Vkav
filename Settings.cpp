#include <algorithm>
#include <ctype.h>
#include <fstream>
#include <unordered_map>
#include <sstream>
#include <string>

#include "Settings.hpp"

std::unordered_map<std::string, std::string> readConfigFile(const char* filePath) {
	std::unordered_map<std::string, std::string> variables;
	std::ifstream configFile(filePath);
	if (!configFile.is_open()) {
		throw std::runtime_error("Failed to open configuration file!");
	}

	std::string line;
	std::string varName;
	std::string varVal;

	while (std::getline(configFile, line)) {
		std::istringstream isLine(line);

		if (!std::getline(isLine, varName, '=')) {
			continue;
		}

		if (varName[0] == '/' && varName[1] == '/') {
			continue;
		}

		if (!std::getline(isLine, varVal)) {
			continue;
		}

		// Remove whitespaces
		varName.resize(std::remove_if(varName.begin(), varName.end(), isspace) - varName.begin());

		// Check for comments
		size_t position;
		if ( (position = varVal.find("//")) != std::string::npos) {
			varVal.erase(position);
		}


		if ( (position = varVal.find('{')) != std::string::npos) { // Check if the input is a list
			varVal = varVal.substr(position, varVal.find('{', position+1)-position+1);
		} else if ( (position = varVal.find('\"')) != std::string::npos) { // Check if the input is a string
			varVal = varVal.substr(position, varVal.find('\"', position+1)-position+1);
		} else {
			// remove whitespaces
			varVal.resize(std::remove_if(varVal.begin(), varVal.end(), isspace) - varVal.begin());
		}

		variables.insert(std::make_pair(varName, varVal));
	}

	configFile.close();

	return variables;
}

std::unordered_map<char, const char*> readCmdLineArgs(int argc, char* argv[]) {
	std::unordered_map<char, const char*> arguments(argc-1);
	for (int i = 1; i < argc; ++i) {
		int size = 0;
		const char* argValue = nullptr;
		std::string argName;
		for (;argv[i][size] != 0; ++size) {
			if (argv[i][size] == '=') {
				argValue = argv[i] + size + 1;
			}

			if (argValue == nullptr) {
				argName.push_back(argv[i][size]);
			}
		}

		if (argv[i][0] != 0 && argv[i][1] != 0 && argv[i][0] == '-') {
			if (argv[i][1] == '-') {
				char charKey = 0;

				if (argName == "--verbose") {charKey = 'v';}
				if (argName == "--sink-name") {charKey = 's';}
				if (argName == "--device") {charKey = 'd';}
				if (argName == "--shader") {charKey = 'S';}
				if (argName == "--config-file") {charKey = 'c';}
				if (argName == "--help") {charKey = 'h';}
				if (argName == "--version") {charKey = 'V';}

				if (charKey == 0) {
					throw std::invalid_argument("Invalid command line argument!");
				}

				arguments.insert(std::make_pair(charKey, argValue));

			} else {
				if (
					argv[i][1] != 'v' &&
					argv[i][1] != 's' &&
					argv[i][1] != 'd' &&
					argv[i][1] != 'S' &&
					argv[i][1] != 'c' &&
					argv[i][1] != 'h' &&
					argv[i][1] != 'V'
				) {
					throw std::invalid_argument("Invalid command line argument!");
				}
				arguments.insert(std::make_pair(argv[i][1], argv[i]+2));
			}
		} else {
			throw std::invalid_argument("Invalid command line argument!");
		}
	}
	return arguments;
}

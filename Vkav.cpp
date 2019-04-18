// C++ standard libraries
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <iomanip>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>

#include "Audio.hpp"
#include "Data.hpp"
#include "Proccess.hpp"
#include "Render.hpp"
#include "Settings.hpp"

enum Device {
	CPU,
	GPU
};

static constexpr const char* versionStr = "Vkav v1.1\n"
                             "Written by Dougal Stewart\n";

static constexpr const char* helpStr = "An audio visualiser using Vulkan for rendering.\n"
                             "\n"
                             "Available Arguments:\n"
                             "\t-v, --verbose                 Prints detailed information about the programs execution.\n"
                             "\t-s, --sink-name=SINK          Uses SINK instead of the default audio sink.(Overrides sink specified in the config file)\n"
                             "\t-d, --device=DEVICE_NUMBER    Uses Device number DEVICE_NUMBER.(Overrides device number specified in the config file.)\n"
                             "\t-c, --config-file=CONFIG_PATH Specifies the path to the configuration file to use\n"
                             "\t-h, --help                    Display this help and exit.\n"
                             "\t-V, --version                 Output version information and exit.\n";

#define PRINT_VAL(p) std::clog << #p << " = " << p << std::endl;
#define PRINT_UNDEFINED(p) std::clog << #p << " not defined!" << std::endl;

class Vkav {
public:
	void run(int argc, char* argv[]) {
		init(argc, argv);
		mainLoop();
		cleanup();
	}

private:

	AudioData audioData;

	AudioSampler audioSampler;
	Renderer renderer;
	Proccess proccess;

	void init(int argc, char* argv[]) {
		// Temporary variables
		float  trebleCut = 0.09f;
		float smoothingLevel = 16.0f;
		size_t smoothedSize  = 320;
		Device smoothingDevice = GPU;

		std::chrono::high_resolution_clock::time_point initStart = std::chrono::high_resolution_clock::now();

		std::unordered_map<char, std::string>::iterator cmdLineArgsIt;
		std::unordered_map<char, std::string> cmdLineArgs = readCmdLineArgs(argc, argv);

		cmdLineArgsIt = cmdLineArgs.find('h');
		if (cmdLineArgsIt != cmdLineArgs.end()) {
			std::cout << "Usage: " << argv[0] << " [OPTIONS]...\n" << helpStr << versionStr;
			std::exit(0);
		}

		cmdLineArgsIt = cmdLineArgs.find('V');
		if (cmdLineArgsIt != cmdLineArgs.end()) {
			std::cout << versionStr;
			std::exit(0);
		}

		cmdLineArgsIt = cmdLineArgs.find('v');
		if (cmdLineArgsIt == cmdLineArgs.end()) {
			std::clog.setstate(std::ios::failbit);
		}

		std::filesystem::path configFilePath = argv[0];
		configFilePath.replace_filename("config");
		cmdLineArgsIt = cmdLineArgs.find('c');
		if (cmdLineArgsIt != cmdLineArgs.end()) {
			configFilePath = cmdLineArgsIt->second;
		}

		std::clog << "Parsing configuration file.\n";
		std::unordered_map<std::string, std::string>::iterator configSettingsIt;
		std::unordered_map<std::string, std::string> configSettings = readConfigFile(configFilePath);

		configSettingsIt = configSettings.find("trebleCut");
		if (configSettingsIt != configSettings.end()) {
			trebleCut = std::stof(configSettingsIt->second);
			PRINT_VAL(trebleCut);
		} else {
			PRINT_UNDEFINED(trebleCut);
		}

		configSettingsIt = configSettings.find("smoothingDevice");
		if (configSettingsIt != configSettings.end()) {
			std::string deviceStr = configSettingsIt->second;
			if (deviceStr == "CPU") {
				smoothingDevice = CPU;
				std::clog << "smoothingDevice = CPU\n";
			} else if (deviceStr == "GPU") {
				smoothingDevice = GPU;
				std::clog << "smoothingDevice = GPU\n";
			} else {
				std::cerr << "Smoothing device set to an invalid value!\n";
			}
		} else {
			PRINT_UNDEFINED(smoothingDevice);
		}

		configSettingsIt = configSettings.find("smoothedSize");
		if (configSettingsIt != configSettings.end()) {
			smoothedSize = std::stoi(configSettingsIt->second);
			PRINT_VAL(smoothedSize);
		} else {
			PRINT_UNDEFINED(smoothedSize);
		}

		AudioSettings audioSettings = {};

		configSettingsIt = configSettings.find("channels");
		if (configSettingsIt != configSettings.end()) {
			audioSettings.channels = std::stoi(configSettingsIt->second);
			std::clog << "audioSettings.channels = " << static_cast<int>(audioSettings.channels) << std::endl;
		} else {
			PRINT_UNDEFINED(audioSettings.channels);
		}

		configSettingsIt = configSettings.find("sampleSize");
		if (configSettingsIt != configSettings.end()) {
			audioSettings.sampleSize = std::stoi(configSettingsIt->second);
			PRINT_VAL(audioSettings.sampleSize);
		} else {
			PRINT_UNDEFINED(audioSettings.sampleSize);
		}

		configSettingsIt = configSettings.find("bufferSize");
		if (configSettingsIt != configSettings.end()) {
			audioSettings.bufferSize = std::stoi(configSettingsIt->second);
			PRINT_VAL(audioSettings.bufferSize);
		} else {
			PRINT_UNDEFINED(audioSettings.bufferSize);
		}

		configSettingsIt = configSettings.find("sampleRate");
		if (configSettingsIt != configSettings.end()) {
			audioSettings.sampleRate = std::stoi(configSettingsIt->second);
			PRINT_VAL(audioSettings.sampleRate);
		} else {
			PRINT_UNDEFINED(audioSettings.sampleRate);
		}

		cmdLineArgsIt = cmdLineArgs.find('s');
		if (cmdLineArgsIt != cmdLineArgs.end()) {
			audioSettings.sinkName = cmdLineArgsIt->second;
			PRINT_VAL(audioSettings.sinkName);
		} else {
			configSettingsIt = configSettings.find("sinkName");
			if (configSettingsIt != configSettings.end()) {
				audioSettings.sinkName = configSettingsIt->second;
				PRINT_VAL(audioSettings.sinkName);
				if (audioSettings.sinkName == "auto") {
					audioSettings.sinkName.clear();
				}
			} else {
				PRINT_UNDEFINED(audioSettings.sinkName);
			}
		}

		std::clog << "Initialising audio.\n";
		audioSampler.start(audioSettings);

		RenderSettings renderSettings = {};

		configSettingsIt = configSettings.find("shaderDirectories");
		if (configSettingsIt != configSettings.end()) {
			renderSettings.shaderDirectories.clear();
			std::stringstream ss(configSettingsIt->second);
			std::string directory;
		 	while (std::getline(ss, directory, '\"').good()) {
				if (!std::getline(ss, directory, '\"').good()) {
					throw std::invalid_argument(__FILE__": Missing terminating \" character in configuration file");
				}
				renderSettings.shaderDirectories.push_back(directory);

			}
			std::clog << "renderSettings.shaderDirectories = {";
			for (const auto& i : renderSettings.shaderDirectories) {
				std::clog << i << ", ";
			}
			std::clog << "\b\b}" << std::endl;
		} else {
			PRINT_UNDEFINED(renderSettings.shaderDirectories);
		}

		configSettingsIt = configSettings.find("backgroundImage");
		if (configSettingsIt != configSettings.end()) {
			renderSettings.backgroundImage = configSettingsIt->second;
			if (renderSettings.backgroundImage == "none") {
				renderSettings.backgroundImage.clear();
				std::clog << "renderSettings.backgroundImage = none\n";
			} else {
				PRINT_VAL(renderSettings.backgroundImage);
			}
		} else {
			PRINT_UNDEFINED(renderSettings.backgroundImage)
		}

		configSettingsIt = configSettings.find("width");
		if (configSettingsIt != configSettings.end()) {
			renderSettings.width = std::stoi(configSettingsIt->second);
			PRINT_VAL(renderSettings.width);
		} else {
			PRINT_UNDEFINED(renderSettings.width);
		}

		configSettingsIt = configSettings.find("height");
		if (configSettingsIt != configSettings.end()) {
			renderSettings.height = std::stoi(configSettingsIt->second);
			PRINT_VAL(renderSettings.height);
		} else {
			PRINT_UNDEFINED(renderSettings.height);
		}

		configSettingsIt = configSettings.find("transparency");
		if (configSettingsIt != configSettings.end()) {
			std::string transparency = configSettingsIt->second;
			#define PRINT(p) std::clog << "renderSettings.transparency = " << #p << std::endl;
			if (transparency == "Vulkan") {
				renderSettings.transparency = VULKAN;
				PRINT(VULKAN)
			} else if (transparency == "Native") {
				renderSettings.transparency = NATIVE;
				PRINT(NATIVE);
			} else if (transparency == "Opaque") {
				renderSettings.transparency = OPAQUE;
				PRINT(OPAQUE);
			} else {
				std::cerr << "Transparency set to an invalid value!\n";
			}
			#undef PRINT
		} else {
			PRINT_UNDEFINED(renderSettings.transparency);
		}

		configSettingsIt = configSettings.find("windowTitle");
		if (configSettingsIt != configSettings.end()) {
			renderSettings.windowTitle = configSettingsIt->second;
			if (renderSettings.windowTitle.find("executable") != std::string::npos) {
				renderSettings.windowTitle = argv[0];
			}
			PRINT_VAL(renderSettings.windowTitle);
		} else {
			PRINT_UNDEFINED(renderSettings.windowTitle);
		}

		configSettingsIt = configSettings.find("windowPosition");
		if (configSettingsIt != configSettings.end()) {
			std::string position = configSettingsIt->second;
			size_t gapPosition = position.find(',');
			renderSettings.windowPosition = {
				std::stoi(position.substr(1, gapPosition-1)),
				std::stoi(position.substr(gapPosition+1, position.size()-gapPosition))
			};
			std::clog << "renderSettings.windowPosition = {"
			          << renderSettings.windowPosition.value().first
					  << ","
					  << renderSettings.windowPosition.value().first
					  << "}"
					  << std::endl;
		} else {
			PRINT_UNDEFINED(renderSettings.windowPosition);
		}

		configSettingsIt = configSettings.find("decorated");
		if (configSettingsIt != configSettings.end()) {
			renderSettings.windowHints.decorated = (configSettingsIt->second == "true");
			PRINT_VAL(renderSettings.windowHints.decorated);
		} else {
			PRINT_UNDEFINED(renderSettings.windowHints.decorated);
		}

		configSettingsIt = configSettings.find("resizable");
		if (configSettingsIt != configSettings.end()) {
			renderSettings.windowHints.resizable = (configSettingsIt->second == "true");
			PRINT_VAL(renderSettings.windowHints.resizable);
		} else {
			PRINT_UNDEFINED(renderSettings.windowHints.resizable);
		}

		configSettingsIt = configSettings.find("smoothingLevel");
		if (configSettingsIt != configSettings.end()) {
			smoothingLevel = std::stof(configSettingsIt->second);
			renderSettings.smoothingLevel = smoothingLevel;
			PRINT_VAL(smoothingLevel);
		} else {
			PRINT_VAL(smoothingLevel);
		}

		switch (smoothingDevice) {
			case GPU:
				renderSettings.audioSize = (audioSettings.bufferSize/2)*(1.f-trebleCut);
				smoothingLevel = 0.f;
				smoothedSize = 0;
				break;
			case CPU:
				renderSettings.audioSize = smoothedSize*(1.f-trebleCut);
				renderSettings.smoothingLevel = 0.f;
				break;
		}

		cmdLineArgsIt = cmdLineArgs.find('d');
		if (cmdLineArgsIt != cmdLineArgs.end()) {
			renderSettings.physicalDevice.value() = std::stoi(cmdLineArgsIt->second);
			std::clog << "renderSettings.physicalDevice = " << renderSettings.physicalDevice.value() << std::endl;
		} else {
			configSettingsIt = configSettings.find("physicalDevice");
			if (configSettingsIt != configSettings.end()) {
				if (configSettingsIt->second == "auto") {
					std::clog << "renderSettings.physicalDevice = auto\n";
				} else {
					renderSettings.physicalDevice.value() = std::stoi(configSettingsIt->second);
					std::clog << "renderSettings.physicalDevice = " << renderSettings.physicalDevice.value() << std::endl;
				}
			} else {
				PRINT_UNDEFINED(renderSettings.physicalDevice);
			}
		}

		std::clog << "Initialising renderer.\n";
		renderer.init(renderSettings);

		ProccessSettings proccessSettings = {};
		proccessSettings.inputSize = audioSettings.bufferSize;
		proccessSettings.outputSize = smoothedSize;
		proccessSettings.smoothingLevel = smoothingLevel;

		configSettingsIt = configSettings.find("amplitude");
		if (configSettingsIt != configSettings.end()) {
			proccessSettings.amplitude = std::stof(configSettingsIt->second);
			PRINT_VAL(proccessSettings.amplitude);
		} else {
			PRINT_UNDEFINED(proccessSettings);
		}

		proccess.init(proccessSettings);

		audioData.allocate(std::max(smoothedSize, audioSettings.bufferSize));

		std::chrono::high_resolution_clock::time_point initEnd = std::chrono::high_resolution_clock::now();
		std::clog << "Initialisation took: " << std::chrono::duration_cast<std::chrono::milliseconds>(initEnd-initStart).count() << " milliseconds\n";
	}

	void mainLoop() {
		int numFrames = 0;
		std::chrono::steady_clock::time_point lastFrame = std::chrono::steady_clock::now();

		while (!audioSampler.stopped) {
			if (audioSampler.modified) {
				audioSampler.copyData(audioData);
				proccess.proccessSignal(audioData);
				if (!renderer.drawFrame(audioData)) {break;}
				++numFrames;
			}
			std::this_thread::sleep_for(std::chrono::microseconds(100));

			std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now();
			if (std::chrono::duration_cast<std::chrono::seconds>(currentTime-lastFrame).count() >= 1) {
				std::clog << "FPS: " << std::setw(3) << std::right << numFrames << " | UPS: " << std::setw(3) << std::right << audioSampler.ups << std::endl;
				numFrames = 0;
				lastFrame = currentTime;
			}

		}
	}

	void cleanup() {
		audioSampler.stop();
		renderer.cleanup();
		proccess.cleanup();
		audioData.deallocate();
	}

};

int main(int argc, char* argv[]) {
	Vkav app;

	try {
		app.run(argc, argv);
	} catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
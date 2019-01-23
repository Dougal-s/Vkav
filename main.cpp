// C++ standard libraries
#include <algorithm>
#include <stdlib.h>
#include <chrono>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include <string>
#include <thread>

#include "Audio.hpp"
#include "Settings.hpp"
#include "Render.hpp"
#include "SignalFunctions.hpp"

static const char* versionStr = "Vkav v1.0\n"
                                "Written by Dougal Stewart\n";

static const char* helpStr = "An audio visualiser using Vulkan for rendering.\n"
                             "\n"
                             "Available Arguments:\n"
                             "\t-v, --verbose                 Prints detailed information about the programs execution.\n"
                             "\t-s, --sink-name=SINK          Uses SINK instead of the default audio sink.(Overrides sink specified in the config file)\n"
                             "\t-d, --device=DEVICE_NUMBER    Uses Device number DEVICE_NUMBER.(Overrides device number specified in the config file.)\n"
                             "\t-S, --shader=SHADER_PATH      Uses fragment shader contained in SHADER_PATH.(Overrides the shader path specified in the config file)\n"
                             "\t-c, --config-file=CONFIG_PATH Specifies the path to the configuration file to use\n"
                             "\t-h, --help                    Display this help and exit.\n"
                             "\t-V, --version                 Output version information and exit.\n";

class AV {
public:
	void run(int argc, char* argv[]) {
		init(argc, argv);
		mainLoop();
		cleanup();
	}

private:

	std::vector<float> lBuffer;
	std::vector<float> rBuffer;

	AudioData audioData;
	Renderer renderer;

	void init(int argc, char* argv[]) {
		// Temporary variables
		float  trebleCut = 0.09f;

		std::chrono::high_resolution_clock::time_point initStart = std::chrono::high_resolution_clock::now();

		std::unordered_map<char, const char*> cmdLineArgs = readCmdLineArgs(argc, argv);
		if (cmdLineArgs.find('h') != cmdLineArgs.end()) {
			std::cout << "Usage: " << argv[0] << "[OPTIONS]...\n" << helpStr << versionStr;
			std::exit(0);
		}

		if (cmdLineArgs.find('V') != cmdLineArgs.end()) {
			std::cout << versionStr;
			std::exit(0);
		}

		std::unordered_map<std::string, std::string> configSettings = readConfigFile("config");

		if (configSettings.find("trebleCut") != configSettings.end()) {
			trebleCut = std::stof(configSettings["trebleCut"]);
			std::cout << "trebleCut = " << trebleCut << std::endl;
		} else {
			std::cerr << "Treble cut not defined!\n";
		}

		AudioSettings audioSettings = {};

		if (configSettings.find("channels") != configSettings.end()) {
			audioSettings.channels = std::stoi(configSettings["channels"]);
			std::cout << "audioSettings.channels = " << static_cast<int>(audioSettings.channels) << std::endl;
		} else {
			std::cerr << "Channel count not defined!\n";
		}

		if (configSettings.find("sampleSize") != configSettings.end()) {
			audioSettings.sampleSize = std::stoi(configSettings["sampleSize"]);
			std::cout << "audioSettings.sampleSize = " << audioSettings.sampleSize << std::endl;
		} else {
			std::cerr << "Sample size not defined!\n";
		}

		if (configSettings.find("bufferSize") != configSettings.end()) {
			audioSettings.bufferSize = std::stoi(configSettings["bufferSize"]);
			std::cout << "audioSettings.bufferSize = " << audioSettings.bufferSize << std::endl;
		} else {
			std::cerr << "Buffer size not defined!\n";
		}

		if (configSettings.find("sampleRate") != configSettings.end()) {
			audioSettings.sampleRate = std::stoi(configSettings["sampleRate"]);
			std::cout << "audioSettings.sampleRate = " << audioSettings.sampleRate << std::endl;
		} else {
			std::cerr << "Sampler rate not defined!\n";
		}

		if (cmdLineArgs.find('s') != cmdLineArgs.end()) {
			audioSettings.sinkName = cmdLineArgs['s'];
			std::cout << "audioSettings.sinkName = " << audioSettings.sinkName << std::endl;
		} else {
			if (configSettings.find("sinkName") != configSettings.end() && configSettings["sinkName"].find("DEFAULT") == std::string::npos) {
				audioSettings.sinkName = configSettings["sinkName"];
				audioSettings.sinkName.erase(0, 1);
				audioSettings.sinkName.pop_back();
				std::cout << "audioSettings.sinkName = " << audioSettings.sinkName << std::endl;
			}
		}

		audioData.begin(audioSettings);

		RendererSettings rendererSettings = {};

		if (cmdLineArgs.find('S') != cmdLineArgs.end()) {
			rendererSettings.shaderPath = cmdLineArgs['S'];
			std::cout << "rendererSettings.shaderPath = " << rendererSettings.shaderPath << std::endl;
		} else {
			if (configSettings.find("shader") != configSettings.end()) {
				rendererSettings.shaderPath = configSettings["shader"];
				rendererSettings.shaderPath.erase(0, 1);
				rendererSettings.shaderPath.pop_back();
				std::cout << "rendererSettings.shaderPath = " << rendererSettings.shaderPath << std::endl;
			} else {
				std::cerr << "Shader module not defined!\n";
			}
		}

		if (configSettings.find("width") != configSettings.end()) {
			rendererSettings.width = std::stoi(configSettings["width"]);
			std::cout << "rendererSettings.width = " << rendererSettings.width << std::endl;
		} else {
			std::cerr << "Window width not defined!\n";
		}

		if (configSettings.find("height") != configSettings.end()) {
			rendererSettings.height = std::stoi(configSettings["height"]);
			std::cout << "rendererSettings.height = " << rendererSettings.height << std::endl;
		} else {
			std::cerr << "Window height not defined!\n";
		}

		if (configSettings.find("transparency") != configSettings.end()) {
			std::string transparency = configSettings["transparency"];
			if (transparency.find("Vulkan") != std::string::npos) {
				rendererSettings.transparency = VULKAN;
			} else if (transparency.find("Native") != std::string::npos) {
				rendererSettings.transparency = NATIVE;
			} else {
				rendererSettings.transparency = OPAQUE;
			}
			std::cout << "rendererSettings.transparency = " << rendererSettings.transparency << std::endl;
		} else {
			std::cerr << "Window transparency not defined!\n";
		}

		rendererSettings.windowTitle = argv[0];
		std::string windowTitle;
		if (configSettings.find("windowTitle") != configSettings.end()) {
			if (configSettings["windowTitle"].find("EXECUTABLE") == std::string::npos) {
				windowTitle = configSettings["windowTitle"];
				rendererSettings.windowTitle = windowTitle.substr(1, windowTitle.size()-2);
			}
			std::cout << "rendererSettings.windowTitle = " << rendererSettings.windowTitle << std::endl;
		} else {
			std::cerr << "Window title not defined!\n";
		}

		if (configSettings.find("windowPosition") != configSettings.end()) {
			std::string position = configSettings["windowPosition"];
			size_t gapPosition = position.find(',');
			rendererSettings.windowPosition = { std::stoi(position.substr(1, gapPosition-1)), std::stoi(position.substr(gapPosition+1, position.size()-gapPosition))};
			std::cout << "rendererSettings.windowPosition = " << std::get<0>(rendererSettings.windowPosition.value()) << "," << std::get<1>(rendererSettings.windowPosition.value()) << std::endl;
		}

		if (configSettings.find("decorated") != configSettings.end()) {
			rendererSettings.windowHints.decorated = (configSettings["decorated"] == "true");
			std::cout << "rendererSettings.windowHints.decorated = " << rendererSettings.windowHints.decorated << std::endl;
		} else {
			std::cerr << "Window hint: \"decorated\" not defined!\n";
		}

		if (configSettings.find("resizable") != configSettings.end()) {
			rendererSettings.windowHints.resizable = (configSettings["resizable"] == "true");
			std::cout << "rendererSettings.windowHints.resizable = " << rendererSettings.windowHints.resizable << std::endl;
		} else {
			std::cerr << "Window hint: \"resizable\" not defined!\n";
		}

		rendererSettings.audioSize = audioSettings.bufferSize/2*(1.f-trebleCut);

		if (configSettings.find("smoothingLevel") != configSettings.end()) {
			rendererSettings.smoothingLevel = std::stof(configSettings["smoothingLevel"]);
			std::cout << "rendererSettings.smoothingLevel = " << rendererSettings.smoothingLevel << std::endl;
		} else {
			std::cerr << "Smoothing level count not defined!\n";
		}

		if (cmdLineArgs.find('d') != cmdLineArgs.end()) {
			rendererSettings.physicalDevice.value() = atoi(cmdLineArgs['d']);
			std::cout << "rendererSettings.physicalDevice = " << rendererSettings.physicalDevice.value() << std::endl;
		} else {
			if (configSettings.find("physicalDevice") != configSettings.end() && configSettings["physicalDevice"].find("DEFAULT") == std::string::npos) {
				rendererSettings.physicalDevice = stoi(configSettings["physicalDevice"]);
				std::cout << "rendererSettings.physicalDevice = " << rendererSettings.physicalDevice.value() << std::endl;
			}
		}

		renderer.init(rendererSettings);

		std::chrono::high_resolution_clock::time_point initEnd = std::chrono::high_resolution_clock::now();
		std::cout << "Initialisation took: " << std::chrono::duration_cast<std::chrono::milliseconds>(initEnd-initStart).count() << " milliseconds\n";
	}

	void mainLoop() {
		int numFrames = 0;
		std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();

		while (!audioData.stop) {
			if (audioData.modified) {
				audioData.copyData(lBuffer, rBuffer);
				windowFunction(lBuffer, rBuffer);
				magnitudes(lBuffer, rBuffer);
				equalise(lBuffer, rBuffer);
				if (!renderer.drawFrame(lBuffer, rBuffer)) {
					break;
				}
				++numFrames;
			}
			std::this_thread::sleep_for(std::chrono::microseconds(1));

		}

		std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
		std::cout << "Avg FPS: " << numFrames*1000.f/std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count() << "\n";
	}

	void cleanup() {
		audioData.end();
		renderer.cleanup();
	}

};

int main(int argc, char* argv[]) {
	AV app;

	try {
		app.run(argc, argv);
	} catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
	}

	return 0;
}

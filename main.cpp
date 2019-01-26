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

enum Device {
	CPU,
	GPU
};

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

	float smoothingLevel = 16.0f;
	size_t smoothedSize  = 320;
	Device smoothingDevice = GPU;

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

		if (cmdLineArgs.find('v') == cmdLineArgs.end()) {
			std::clog.setstate(std::ios::failbit);
		}

		std::string configFilePath = "config";
		if (cmdLineArgs.find('c') != cmdLineArgs.end()) {
			configFilePath = cmdLineArgs['c'];
		}

		std::clog << "Parsing configuration file.\n";
		std::unordered_map<std::string, std::string> configSettings = readConfigFile(configFilePath);

		if (configSettings.find("trebleCut") != configSettings.end()) {
			trebleCut = std::stof(configSettings["trebleCut"]);
			std::clog << "trebleCut = " << trebleCut << std::endl;
		} else {
			std::clog << "Treble cut not defined!\n";
		}

		if (configSettings.find("smoothingDevice") != configSettings.end()) {
			std::string deviceStr = configSettings["smoothingDevice"];
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
			std::clog << "Smoothing device not defined!\n";
		}

		if (smoothingDevice == CPU) {
			if (configSettings.find("smoothedSize") != configSettings.end()) {
				smoothedSize = std::stoi(configSettings["smoothedSize"]);
				std::clog << "smoothedSize = " << smoothedSize << std::endl;
			} else {
				std::clog << "Smoothed size not defined!\n";
			}
		}

		AudioSettings audioSettings = {};

		if (configSettings.find("channels") != configSettings.end()) {
			audioSettings.channels = std::stoi(configSettings["channels"]);
			std::clog << "audioSettings.channels = " << static_cast<int>(audioSettings.channels) << std::endl;
		} else {
			std::clog << "Channel count not defined!\n";
		}

		if (configSettings.find("sampleSize") != configSettings.end()) {
			audioSettings.sampleSize = std::stoi(configSettings["sampleSize"]);
			std::clog << "audioSettings.sampleSize = " << audioSettings.sampleSize << std::endl;
		} else {
			std::clog << "Sample size not defined!\n";
		}

		if (configSettings.find("bufferSize") != configSettings.end()) {
			audioSettings.bufferSize = std::stoi(configSettings["bufferSize"]);
			std::clog << "audioSettings.bufferSize = " << audioSettings.bufferSize << std::endl;
		} else {
			std::clog << "Buffer size not defined!\n";
		}

		if (configSettings.find("sampleRate") != configSettings.end()) {
			audioSettings.sampleRate = std::stoi(configSettings["sampleRate"]);
			std::clog << "audioSettings.sampleRate = " << audioSettings.sampleRate << std::endl;
		} else {
			std::clog << "Sample rate not defined!\n";
		}

		if (cmdLineArgs.find('s') != cmdLineArgs.end()) {
			audioSettings.sinkName = cmdLineArgs['s'];
			std::clog << "audioSettings.sinkName = " << audioSettings.sinkName << std::endl;
		} else {
			if (configSettings.find("sinkName") != configSettings.end()) {
				if (configSettings["sinkName"].find("auto") != std::string::npos) {
					std::clog << "audioSettings.sinkName = auto" << std::endl;
				} else {
					audioSettings.sinkName = configSettings["sinkName"];
					std::clog << "audioSettings.sinkName = " << audioSettings.sinkName << std::endl;
				}
			} else {
				std::clog << "PulseAudio sink name not defined!\n";
			}
		}

		std::clog << "Initialising audio.\n";
		audioData.begin(audioSettings);

		RendererSettings rendererSettings = {};

		if (cmdLineArgs.find('S') != cmdLineArgs.end()) {
			rendererSettings.shaderPath = cmdLineArgs['S'];
			std::clog << "rendererSettings.shaderPath = " << rendererSettings.shaderPath << std::endl;
		} else {
			if (configSettings.find("shader") != configSettings.end()) {
				rendererSettings.shaderPath = configSettings["shader"];
				std::clog << "rendererSettings.shaderPath = " << rendererSettings.shaderPath << std::endl;
			} else {
				std::clog << "Shader path not defined!\n";
			}
		}

		if (configSettings.find("width") != configSettings.end()) {
			rendererSettings.width = std::stoi(configSettings["width"]);
			std::clog << "rendererSettings.width = " << rendererSettings.width << std::endl;
		} else {
			std::clog << "Window width not defined!\n";
		}

		if (configSettings.find("height") != configSettings.end()) {
			rendererSettings.height = std::stoi(configSettings["height"]);
			std::clog << "rendererSettings.height = " << rendererSettings.height << std::endl;
		} else {
			std::clog << "Window height not defined!\n";
		}

		if (configSettings.find("transparency") != configSettings.end()) {
			std::string transparency = configSettings["transparency"];
			if (transparency.find("Vulkan") != std::string::npos) {
				rendererSettings.transparency = VULKAN;
			} else if (transparency.find("Native") != std::string::npos) {
				rendererSettings.transparency = NATIVE;
			} else if (transparency.find("Opaque") != std::string::npos) {
				rendererSettings.transparency = OPAQUE;
			} else {
				std::cerr << "Transparency set to an invalid value!\n";
			}
			std::clog << "rendererSettings.transparency = " << rendererSettings.transparency << std::endl;
		} else {
			std::clog << "Window transparency not defined!\n";
		}

		if (configSettings.find("windowTitle") != configSettings.end()) {
			if (configSettings["windowTitle"].find("executable") != std::string::npos) {
				rendererSettings.windowTitle = argv[0];
			} else {
				rendererSettings.windowTitle = configSettings["windowTitle"];
			}
			std::clog << "rendererSettings.windowTitle = " << rendererSettings.windowTitle << std::endl;
		} else {
			std::clog << "Window title not defined!\n";
		}

		if (configSettings.find("windowPosition") != configSettings.end()) {
			std::string position = configSettings["windowPosition"];
			size_t gapPosition = position.find(',');
			rendererSettings.windowPosition = {std::stoi(position.substr(1, gapPosition-1)), std::stoi(position.substr(gapPosition+1, position.size()-gapPosition))};
			std::clog << "rendererSettings.windowPosition = " << rendererSettings.windowPosition.value().first << "," << rendererSettings.windowPosition.value().first << std::endl;
		} else {
			std::clog << "Window postition not defined!\n";
		}

		if (configSettings.find("decorated") != configSettings.end()) {
			rendererSettings.windowHints.decorated = (configSettings["decorated"] == "true");
			std::clog << "rendererSettings.windowHints.decorated = " << rendererSettings.windowHints.decorated << std::endl;
		} else {
			std::clog << "Window hint: \"decorated\" not defined!\n";
		}

		if (configSettings.find("resizable") != configSettings.end()) {
			rendererSettings.windowHints.resizable = (configSettings["resizable"] == "true");
			std::clog << "rendererSettings.windowHints.resizable = " << rendererSettings.windowHints.resizable << std::endl;
		} else {
			std::clog << "Window hint: \"resizable\" not defined!\n";
		}

		if (smoothingDevice == GPU) {
			rendererSettings.audioSize = (audioSettings.bufferSize/2)*(1.f-trebleCut);
		} else if (smoothingDevice == CPU) {
			rendererSettings.audioSize = smoothedSize*(1.f-trebleCut);
		}

		if (smoothingDevice == CPU)
			rendererSettings.smoothingLevel = 0.f;

		if (configSettings.find("smoothingLevel") != configSettings.end()) {
			if (smoothingDevice == CPU) {
				smoothingLevel = std::stof(configSettings["smoothingLevel"]);
				std::clog << "smoothingLevel = " << smoothingLevel << std::endl;
			} else if (smoothingDevice == GPU) {
				rendererSettings.smoothingLevel = std::stof(configSettings["smoothingLevel"]);
				std::clog << "rendererSettings.smoothingLevel = " << rendererSettings.smoothingLevel << std::endl;
			}
		} else {
			std::clog << "Smoothing level not defined!\n";
		}

		if (cmdLineArgs.find('d') != cmdLineArgs.end()) {
			rendererSettings.physicalDevice.value() = atoi(cmdLineArgs['d']);
			std::clog << "rendererSettings.physicalDevice = " << rendererSettings.physicalDevice.value() << std::endl;
		} else {
			if (configSettings.find("physicalDevice") != configSettings.end()) {
				if (configSettings["physicalDevice"].find("auto") != std::string::npos) {
					std::clog << "rendererSettings.physicalDevice = auto" << std::endl;
				} else {
					rendererSettings.physicalDevice.value() = stoi(configSettings["physicalDevice"]);
					std::clog << "rendererSettings.physicalDevice = " << rendererSettings.physicalDevice.value() << std::endl;
				}
			} else {
				std::clog << "Physical device number not defined!\n";
			}
		}

		std::clog << "Initialising renderer.\n";
		renderer.init(rendererSettings);

		std::chrono::high_resolution_clock::time_point initEnd = std::chrono::high_resolution_clock::now();
		std::clog << "Initialisation took: " << std::chrono::duration_cast<std::chrono::milliseconds>(initEnd-initStart).count() << " milliseconds\n";
	}

	void mainLoop() {
		std::clog << "Entering main loop.\n";
		int numFrames = 0;
		std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();

		while (!audioData.stop) {
			if (audioData.modified) {
				audioData.copyData(lBuffer, rBuffer);
				windowFunction(lBuffer, rBuffer);
				magnitudes(lBuffer, rBuffer);
				equalise(lBuffer, rBuffer);
				if (smoothingDevice == CPU) {
					smooth(lBuffer, rBuffer, smoothedSize, smoothingLevel);
				}
				if (!renderer.drawFrame(lBuffer, rBuffer)) {
					break;
				}
				++numFrames;
			}
			std::this_thread::sleep_for(std::chrono::microseconds(1));

		}
		std::clog << "Exiting main loop.\n";
		std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
		std::cout << "Avg FPS: " << numFrames*1000.f/std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count() << std::endl;
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

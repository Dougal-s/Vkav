// C++ standard libraries
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
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

enum Device { CPU, GPU };

static constexpr const char* versionStr =
    "Vkav v1.1\n"
    "Written by Dougal Stewart\n";

static constexpr const char* helpStr =
    "An audio visualiser using Vulkan for rendering.\n"
    "\n"
    "Available Arguments:\n"
    "\t-v, --verbose                 Prints detailed information about the "
    "programs execution.\n"
    "\t-s, --sink-name=SINK          Uses SINK instead of the default audio "
    "sink.(Overrides sink specified in the config file.)\n"
    "\t-d, --device=DEVICE_NUMBER    Uses Device number "
    "DEVICE_NUMBER.(Overrides device number specified in the config file.)\n"
    "\t-c, --config-file=CONFIG_PATH Specifies the path to the configuration "
    "file to use.\n"
    "\t-a, --amplitude=AMPLITUDE     Changes the amplitude of the fft output.\n"
    "\t-h, --help                    Display this help and exit.\n"
    "\t-V, --version                 Output version information and exit.\n";

#define PRINT_UNDEFINED(name) \
	std::clog << #name << " not defined!" << std::endl;

class Vkav {
public:
	Vkav(int argc, char* argv[]) {
		// Temporary variables
		float trebleCut = 0.09f;
		float smoothingLevel = 16.0f;
		size_t smoothedSize = 320;
		Device smoothingDevice = GPU;

		std::chrono::high_resolution_clock::time_point initStart =
		    std::chrono::high_resolution_clock::now();

		std::unordered_map<char, std::string> cmdLineArgs =
		    readCmdLineArgs(argc, argv);

		if (cmdLineArgs.find('h') != cmdLineArgs.end()) {
			std::cout << "Usage: " << argv[0] << " [OPTIONS]...\n"
			          << helpStr << versionStr;
			std::exit(0);
		}

		if (cmdLineArgs.find('V') != cmdLineArgs.end()) {
			std::cout << versionStr;
			std::exit(0);
		}

		if (cmdLineArgs.find('v') == cmdLineArgs.end())
			std::clog.setstate(std::ios::failbit);

		std::filesystem::path configFilePath = argv[0];
		configFilePath.replace_filename("config");
		if (const auto cmdLineArg = cmdLineArgs.find('c');
		    cmdLineArg != cmdLineArgs.end())
			configFilePath = cmdLineArg->second;

		std::clog << "Parsing configuration file.\n";
		std::unordered_map<std::string, std::string> configSettings =
		    readConfigFile(configFilePath);

		if (const auto confSetting = configSettings.find("trebleCut");
		    confSetting != configSettings.end())
			trebleCut = std::stof(confSetting->second);
		else
			PRINT_UNDEFINED(trebleCut);

		if (const auto confSetting = configSettings.find("smoothingDevice");
		    confSetting != configSettings.end()) {
			std::string deviceStr = confSetting->second;
			if (deviceStr == "CPU") {
				smoothingDevice = CPU;
			} else if (deviceStr == "GPU") {
				smoothingDevice = GPU;
			} else {
				std::cerr << "Smoothing device set to an invalid value!\n";
			}
		} else {
			PRINT_UNDEFINED(smoothingDevice);
		}

		if (const auto confSetting = configSettings.find("smoothedSize");
		    confSetting != configSettings.end())
			smoothedSize = std::stoi(confSetting->second);
		else
			PRINT_UNDEFINED(smoothedSize);

		AudioSettings audioSettings = {};

		if (const auto confSetting = configSettings.find("channels");
		    confSetting != configSettings.end())
			audioSettings.channels = std::stoi(confSetting->second);
		else
			PRINT_UNDEFINED(channels);

		if (const auto confSetting = configSettings.find("sampleSize");
		    confSetting != configSettings.end())
			audioSettings.sampleSize = std::stoi(confSetting->second);
		else
			PRINT_UNDEFINED(sampleSize);

		if (const auto confSetting = configSettings.find("bufferSize");
		    confSetting != configSettings.end())
			audioSettings.bufferSize = std::stoi(confSetting->second);
		else
			PRINT_UNDEFINED(bufferSize);

		if (const auto confSetting = configSettings.find("sampleRate");
		    confSetting != configSettings.end())
			audioSettings.sampleRate = std::stoi(confSetting->second);
		else
			PRINT_UNDEFINED(sampleRate);

		if (const auto cmdLineArg = cmdLineArgs.find('s');
		    cmdLineArg != cmdLineArgs.end()) {
			audioSettings.sinkName = cmdLineArg->second;
		} else {
			if (const auto confSetting = configSettings.find("sinkName");
			    confSetting != configSettings.end()) {
				audioSettings.sinkName = confSetting->second;
				if (audioSettings.sinkName == "auto")
					audioSettings.sinkName.clear();
			} else {
				PRINT_UNDEFINED(sinkName);
			}
		}

		std::clog << "Initialising audio.\n";
		audioSampler.start(audioSettings);

		RenderSettings renderSettings = {};

		if (const auto confSetting = configSettings.find("shaderDirectories");
		    confSetting != configSettings.end()) {
			renderSettings.shaderDirectories.clear();
			std::stringstream ss(confSetting->second);
			std::string directory;
			while (std::getline(ss, directory, '\"').good()) {
				if (!std::getline(ss, directory, '\"').good())
					throw std::invalid_argument(
					    __FILE__
					    ": Missing terminating \" character in configuration "
					    "file");
				renderSettings.shaderDirectories.push_back(directory);
			}
		} else {
			PRINT_UNDEFINED(shaderDirectories);
		}

		if (const auto confSetting = configSettings.find("backgroundImage");
		    confSetting != configSettings.end()) {
			renderSettings.backgroundImage = confSetting->second;
			if (renderSettings.backgroundImage == "none")
				renderSettings.backgroundImage.clear();
		} else {
			PRINT_UNDEFINED(backgroundImage)
		}

		if (const auto confSetting = configSettings.find("width");
		    confSetting != configSettings.end())
			renderSettings.width = std::stoi(confSetting->second);
		else
			PRINT_UNDEFINED(width);

		if (const auto confSetting = configSettings.find("height");
		    confSetting != configSettings.end())
			renderSettings.height = std::stoi(confSetting->second);
		else
			PRINT_UNDEFINED(height);

		if (const auto confSetting = configSettings.find("transparency");
		    confSetting != configSettings.end()) {
			std::string transparency = confSetting->second;
			if (transparency == "Vulkan")
				renderSettings.transparency = VULKAN;
			else if (transparency == "Native")
				renderSettings.transparency = NATIVE;
			else if (transparency == "Opaque")
				renderSettings.transparency = OPAQUE;
			else
				std::cerr << "Transparency set to an invalid value!\n";
		} else {
			PRINT_UNDEFINED(transparency);
		}

		if (const auto confSetting = configSettings.find("windowTitle");
		    confSetting != configSettings.end()) {
			renderSettings.windowTitle = confSetting->second;
			if (renderSettings.windowTitle.find("executable") !=
			    std::string::npos)
				renderSettings.windowTitle = argv[0];
		} else {
			PRINT_UNDEFINED(windowTitle);
		}

		if (const auto confSetting = configSettings.find("windowPosition");
		    confSetting != configSettings.end()) {
			std::string position = confSetting->second;
			size_t gapPosition = position.find(',');
			renderSettings.windowPosition = {
			    std::stoi(position.substr(1, gapPosition - 1)),
			    std::stoi(position.substr(gapPosition + 1,
			                              position.size() - gapPosition))};
		} else {
			PRINT_UNDEFINED(windowPosition);
		}

		if (const auto confSetting = configSettings.find("decorated");
		    confSetting != configSettings.end())
			renderSettings.windowHints.decorated =
			    (confSetting->second == "true");
		else
			PRINT_UNDEFINED(decorated);

		if (const auto confSetting = configSettings.find("resizable");
		    confSetting != configSettings.end())
			renderSettings.windowHints.resizable =
			    (confSetting->second == "true");
		else
			PRINT_UNDEFINED(resizable);

		if (const auto confSetting = configSettings.find("smoothingLevel");
		    confSetting != configSettings.end()) {
			smoothingLevel = std::stof(confSetting->second);
			renderSettings.smoothingLevel = smoothingLevel;
		} else {
			PRINT_UNDEFINED(smoothingLevel);
		}

		switch (smoothingDevice) {
			case GPU:
				renderSettings.audioSize =
				    (audioSettings.bufferSize / 2) * (1.f - trebleCut);
				smoothingLevel = 0.f;
				smoothedSize = 0;
				break;
			case CPU:
				renderSettings.audioSize = smoothedSize * (1.f - trebleCut);
				renderSettings.smoothingLevel = 0.f;
				break;
		}

		if (const auto cmdLineArg = cmdLineArgs.find('d');
		    cmdLineArg != cmdLineArgs.end()) {
			renderSettings.physicalDevice.value() =
			    std::stoi(cmdLineArg->second);
		} else {
			if (const auto confSetting = configSettings.find("physicalDevice");
			    confSetting != configSettings.end()) {
				if (confSetting->second != "auto")
					renderSettings.physicalDevice.value() =
					    std::stoi(confSetting->second);
			} else {
				PRINT_UNDEFINED(physicalDevice);
			}
		}

		std::clog << "Initialising renderer.\n";
		renderer.init(renderSettings);

		ProccessSettings proccessSettings = {};
		proccessSettings.channels = audioSettings.channels;
		proccessSettings.inputSize = audioSettings.bufferSize;
		proccessSettings.outputSize = smoothedSize;
		proccessSettings.smoothingLevel = smoothingLevel;

		if (const auto cmdLineArg = cmdLineArgs.find('a');
		    cmdLineArg != cmdLineArgs.end()) {
			proccessSettings.amplitude = std::stof(cmdLineArg->second);
		} else {
			if (const auto confSetting = configSettings.find("amplitude");
			    confSetting != configSettings.end())
				proccessSettings.amplitude = std::stof(confSetting->second);
			else
				PRINT_UNDEFINED(amplitude);
		}

		proccess.init(proccessSettings);

		audioData.allocate(
		    audioSettings.channels * audioSettings.bufferSize,
		    std::max(smoothedSize, audioSettings.bufferSize / 2));

		std::chrono::high_resolution_clock::time_point initEnd =
		    std::chrono::high_resolution_clock::now();
		std::clog << "Initialisation took: "
		          << std::chrono::duration_cast<std::chrono::milliseconds>(
		                 initEnd - initStart)
		                 .count()
		          << " milliseconds\n";
	}

	void run() {
		mainLoop();
		cleanup();
	}

private:
	AudioData audioData;

	AudioSampler audioSampler;
	Renderer renderer;
	Proccess proccess;

	void mainLoop() {
		int numFrames = 0;
		std::chrono::steady_clock::time_point lastFrame =
		    std::chrono::steady_clock::now();

		while (!audioSampler.stopped()) {
			if (audioSampler.modified()) {
				audioSampler.copyData(audioData);
				proccess.proccessSignal(audioData);
				if (!renderer.drawFrame(audioData)) break;
				++numFrames;
			}
			std::this_thread::sleep_for(std::chrono::microseconds(100));

			std::chrono::steady_clock::time_point currentTime =
			    std::chrono::steady_clock::now();
			if (std::chrono::duration_cast<std::chrono::seconds>(currentTime -
			                                                     lastFrame)
			        .count() >= 1) {
				std::clog << "FPS: " << std::setw(3) << std::right << numFrames
				          << " | UPS: " << std::setw(3) << std::right
				          << audioSampler.ups() << std::endl;
				numFrames = 0;
				lastFrame = currentTime;
			}
		}

		// rethrow any exceptions the audio thread may have thrown
		audioSampler.rethrowExceptions();
	}

	void cleanup() {
		audioSampler.stop();
		renderer.cleanup();
		proccess.cleanup();
	}
};

int main(int argc, char* argv[]) {
	Vkav app(argc, argv);

	try {
		app.run();
	} catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

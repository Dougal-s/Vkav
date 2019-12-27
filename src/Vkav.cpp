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

#define S1(x) #x
#define S2(x) S1(x)
#define LOCATION __FILE__ ":" S2(__LINE__) ": "

namespace {

	enum Device { CPU, GPU };

	static constexpr const char* versionStr =
	    "Vkav v1.1 "
#ifdef NDEBUG
	    "release\n"
#else
	    "debug\n"
#endif
	    "Written by Dougal Stewart\n";

	static constexpr const char* helpStr =
	    "An audio visualiser using Vulkan for rendering.\n"
	    "\n"
	    "Available Arguments:\n"
	    "-v, --verbose                 Prints detailed information about\n"
	    "                                program execution.\n"
	    "-s, --sink-name=SINK          Use SINK instead of the default audio\n"
	    "                                sink. Overrides config file.\n"
	    "-d, --device=DEVICE_NUMBER    Use Device number DEVICE_NUMBER.\n"
	    "                                Overrides config file.\n"
	    "-c, --config=CONFIG_PATH      Specifies config file path.\n"
	    "-a, --amplitude=AMPLITUDE     Multiplies audio with AMPLITUDE.\n"
	    "-h, --help                    Display this help and exit.\n"
	    "-V, --version                 Output version information and exit.\n"
	    "    --install-config          Installs config files to a user\n"
	    "                                specific config directory.\n"
	    "\n";

#define PRINT_UNDEFINED(name) std::clog << #name << " not defined!" << std::endl;

	class Vkav {
	public:
		Vkav(int argc, const char* argv[]) {
			std::chrono::high_resolution_clock::time_point initStart =
			    std::chrono::high_resolution_clock::now();

			const std::unordered_map<std::string, std::string> cmdLineArgs =
			    readCmdLineArgs(argc, argv);

			if (cmdLineArgs.find("help") != cmdLineArgs.end()) {
				std::cout << "Usage: " << argv[0] << " [OPTIONS]...\n" << helpStr << versionStr;
				std::exit(0);
			}

			if (cmdLineArgs.find("version") != cmdLineArgs.end()) {
				std::cout << versionStr;
				std::exit(0);
			}

			if (cmdLineArgs.find("install-config") != cmdLineArgs.end()) {
				installConfig();
				std::exit(0);
			}

			if (cmdLineArgs.find("verbose") == cmdLineArgs.end())
				std::clog.setstate(std::ios::failbit);

			std::filesystem::path configFilePath;

#ifdef NDEBUG
			auto configLocations = getConfigLocations();
#else
			std::vector<std::filesystem::path> configLocations = {argv[0]};
			configLocations.front().remove_filename();
			configLocations.front() = std::filesystem::canonical(configLocations.front()).parent_path()/"src";
#endif
			for (auto& path : configLocations) {
				if (std::filesystem::exists(path / "config")) {
					configFilePath = path / "config";
					break;
				}
			}

			if (const auto cmdLineArg = cmdLineArgs.find("config"); cmdLineArg != cmdLineArgs.end())
				configFilePath = cmdLineArg->second;


			std::clog << "Parsing configuration file.\n";
			const std::unordered_map<std::string, std::string> configSettings =
			    readConfigFile(configFilePath);

			AudioSettings audioSettings = {};
			RenderSettings renderSettings = {};
			renderSettings.configLocations = configLocations;
			ProccessSettings proccessSettings = {};

			fillStructs(argv[0], cmdLineArgs, configSettings, audioSettings, renderSettings,
			            proccessSettings);

			std::clog << "Initialising audio.\n";
			audioSampler.start(audioSettings);
			std::clog << "Initialising renderer.\n";
			renderer.init(renderSettings);
			proccess.init(proccessSettings);

			audioData.allocate(audioSettings.channels * audioSettings.bufferSize,
			                   std::max(proccessSettings.outputSize, audioSettings.bufferSize / 2));

			std::chrono::high_resolution_clock::time_point initEnd =
			    std::chrono::high_resolution_clock::now();
			std::clog << "Initialisation took: "
			          << std::chrono::duration_cast<std::chrono::milliseconds>(initEnd - initStart)
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
			std::chrono::microseconds targetFrameTime(1000000 / 88);
			std::chrono::high_resolution_clock::time_point lastFrame =
			    std::chrono::high_resolution_clock::now();
			std::chrono::steady_clock::time_point lastUpdate = std::chrono::steady_clock::now();

			while (audioSampler.running()) {
				if (audioSampler.modified()) {
					lastFrame = std::chrono::high_resolution_clock::now();
					audioSampler.copyData(audioData);
					proccess.proccessSignal(audioData);
					if (!renderer.drawFrame(audioData)) break;
					++numFrames;
					std::this_thread::sleep_until(lastFrame + targetFrameTime / 2);
				}
				std::this_thread::sleep_for(targetFrameTime / 4);

				std::chrono::steady_clock::time_point currentTime =
				    std::chrono::steady_clock::now();
				if (std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastUpdate)
				        .count() >= 1) {
					std::clog << "FPS: " << std::setw(3) << std::right << numFrames
					          << " | UPS: " << std::setw(3) << std::right << audioSampler.ups()
					          << std::endl;
					numFrames = 0;
					lastUpdate = currentTime;
					targetFrameTime = std::chrono::microseconds(1000000 / audioSampler.ups());
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

		static void fillStructs(const char* execPath,
		                        const std::unordered_map<std::string, std::string> cmdLineArgs,
		                        const std::unordered_map<std::string, std::string> configSettings,
		                        AudioSettings& audioSettings, RenderSettings& renderSettings,
		                        ProccessSettings& proccessSettings) {
			float trebleCut = 0.09f;
			if (const auto confSetting = configSettings.find("trebleCut");
			    confSetting != configSettings.end())
				trebleCut = std::stof(confSetting->second);
			else
				PRINT_UNDEFINED(trebleCut);

			Device smoothingDevice = GPU;
			if (const auto confSetting = configSettings.find("smoothingDevice");
			    confSetting != configSettings.end()) {
				std::string deviceStr = confSetting->second;
				if (deviceStr == "CPU") {
					smoothingDevice = CPU;
				} else if (deviceStr == "GPU") {
					smoothingDevice = GPU;
				} else {
					std::cerr << LOCATION "Smoothing device set to an invalid value!\n";
				}
			} else {
				PRINT_UNDEFINED(smoothingDevice);
			}

			size_t smoothedSize = 320;
			if (const auto confSetting = configSettings.find("smoothedSize");
			    confSetting != configSettings.end())
				smoothedSize = std::stoi(confSetting->second);
			else
				PRINT_UNDEFINED(smoothedSize);

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

			if (const auto cmdLineArg = cmdLineArgs.find("sink-name");
			    cmdLineArg != cmdLineArgs.end()) {
				audioSettings.sinkName = cmdLineArg->second;
			} else {
				if (const auto confSetting = configSettings.find("sinkName");
				    confSetting != configSettings.end()) {
					audioSettings.sinkName = confSetting->second;
					if (audioSettings.sinkName == "auto") audioSettings.sinkName.clear();
				} else {
					PRINT_UNDEFINED(sinkName);
				}
			}

			if (const auto confSetting = configSettings.find("moduleDirectories");
			    confSetting != configSettings.end()) {
				renderSettings.modules.clear();
				std::stringstream ss(confSetting->second);
				std::string directory;
				while (std::getline(ss, directory, '\"').good()) {
					if (!std::getline(ss, directory, '\"').good())
						throw std::invalid_argument(__FILE__
						                            ": Missing terminating \" character in "
						                            "configuration "
						                            "file");
					renderSettings.modules.push_back(directory);
				}
			} else {
				PRINT_UNDEFINED(moduleDirectories);
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
					std::cerr << LOCATION "Transparency set to an invalid value!\n";
			} else {
				PRINT_UNDEFINED(transparency);
			}

			if (const auto confSetting = configSettings.find("windowTitle");
			    confSetting != configSettings.end()) {
				renderSettings.windowTitle = confSetting->second;
				if (renderSettings.windowTitle.find("executable") != std::string::npos)
					renderSettings.windowTitle = execPath;
			} else {
				PRINT_UNDEFINED(windowTitle);
			}

			if (const auto confSetting = configSettings.find("windowPosition");
			    confSetting != configSettings.end()) {
				std::string position = confSetting->second;
				size_t gapPosition = position.find(',');
				renderSettings.windowPosition = {
				    std::stoi(position.substr(1, gapPosition - 1)),
				    std::stoi(position.substr(gapPosition + 1, position.size() - gapPosition))};
			} else {
				PRINT_UNDEFINED(windowPosition);
			}

			if (const auto confSetting = configSettings.find("decorated");
			    confSetting != configSettings.end())
				renderSettings.windowHints.decorated = (confSetting->second == "true");
			else
				PRINT_UNDEFINED(decorated);

			if (const auto confSetting = configSettings.find("resizable");
			    confSetting != configSettings.end())
				renderSettings.windowHints.resizable = (confSetting->second == "true");
			else
				PRINT_UNDEFINED(resizable);

			float smoothingLevel = 16.0f;
			if (const auto confSetting = configSettings.find("smoothingLevel");
			    confSetting != configSettings.end()) {
				smoothingLevel = std::stof(confSetting->second);
				renderSettings.smoothingLevel = smoothingLevel;
			} else {
				PRINT_UNDEFINED(smoothingLevel);
			}

			switch (smoothingDevice) {
				case GPU:
					renderSettings.audioSize = (audioSettings.bufferSize / 2) * (1.f - trebleCut);
					smoothingLevel = 0.f;
					smoothedSize = 0;
					break;
				case CPU:
					renderSettings.audioSize = smoothedSize * (1.f - trebleCut);
					renderSettings.smoothingLevel = 0.f;
					break;
			}

			if (const auto cmdLineArg = cmdLineArgs.find("device");
			    cmdLineArg != cmdLineArgs.end()) {
				renderSettings.physicalDevice.value() = std::stoi(cmdLineArg->second);
			} else {
				if (const auto confSetting = configSettings.find("physicalDevice");
				    confSetting != configSettings.end()) {
					if (confSetting->second != "auto")
						renderSettings.physicalDevice.value() = std::stoi(confSetting->second);
				} else {
					PRINT_UNDEFINED(physicalDevice);
				}
			}

			proccessSettings.channels = audioSettings.channels;
			proccessSettings.inputSize = audioSettings.bufferSize;
			proccessSettings.outputSize = smoothedSize;
			proccessSettings.smoothingLevel = smoothingLevel;

			if (const auto cmdLineArg = cmdLineArgs.find("amplitude");
			    cmdLineArg != cmdLineArgs.end()) {
				proccessSettings.amplitude = std::stof(cmdLineArg->second);
			} else {
				if (const auto confSetting = configSettings.find("amplitude");
				    confSetting != configSettings.end())
					proccessSettings.amplitude = std::stof(confSetting->second);
				else
					PRINT_UNDEFINED(amplitude);
			}
		}
	};
}  // namespace

int main(int argc, const char* argv[]) {
	try {
		Vkav app(argc, argv);
		app.run();
	} catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

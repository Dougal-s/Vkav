// C++ standard libraries
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>

#include "Audio.hpp"
#include "Calculate.hpp"
#include "Data.hpp"
#include "Proccess.hpp"
#include "Render.hpp"
#include "Settings.hpp"
#include "Version.hpp"

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define LOCATION __FILE__ ":" STR(__LINE__) ": "

namespace {

	enum class Device { cpu, gpu };

	static constexpr const char* versionStr =
	    "Vkav v" STR(VERSION_MAJOR) "." STR(VERSION_MINOR) "." STR(VERSION_PATCH) " "
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
	    "-v, --verbose                         Prints detailed information about\n"
	    "                                        program execution.\n"
	    "-s, --sinkName=SINK                   Use SINK instead of the default audio\n"
	    "                                        sink. Overrides config file.\n"
	    "-d, --physicalDevice=DEVICE_NUMBER    Use device number DEVICE_NUMBER.\n"
	    "                                        Overrides config file.\n"
	    "-c, --config=CONFIG_PATH              Specifies config file path.\n"
	    "-a, --amplitude=AMPLITUDE             Multiplies audio with AMPLITUDE.\n"
	    "    --install-config                  Installs config files to a user\n"
	    "    --list-modules                    Output the list of available modules and exit\n"
	    "-h, --help                            Display this help and exit.\n"
	    "-V, --version                         Output version information and exit.\n"
	    "                                        specific config directory.\n"
	    "\n"
	    "Any of the settings in the config file can be set using the format:\n"
	    "--SETTINGNAME=\"VALUE\"\n"
	    "\n";

#define WARN_UNDEFINED(name) std::clog << #name << " not defined!" << std::endl;

	class Vkav {
	public:
		Vkav(int argc, const char* argv[]) {
			auto initStart = std::chrono::high_resolution_clock::now();

			std::unordered_map<std::string, std::string> cmdLineArgs = readCmdLineArgs(argc, argv);

			if (cmdLineArgs.find("help") != cmdLineArgs.end()) {
				std::cout << "Usage: " << argv[0] << " [OPTIONS]...\n" << helpStr << versionStr;
				std::exit(0);
			}

			if (cmdLineArgs.find("version") != cmdLineArgs.end()) {
				std::cout << versionStr;
				std::exit(0);
			}

			if (cmdLineArgs.find("list-modules") != cmdLineArgs.end()) {
				auto modules = getModules();
				std::cout << "Available Modules:" << std::endl;
				for (auto& module : modules) {
					std::cout << module.first << ":\n";
					std::cout << "    location(s):\n";
					for (auto& path : module.second) std::cout << "        " << path << std::endl;
				}
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
			configLocations.front() =
			    std::filesystem::canonical(configLocations.front()).parent_path() / "src";
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
			cmdLineArgs.merge(readConfigFile(configFilePath));

			AudioSettings audioSettings = {};
			RenderSettings renderSettings = {};
			renderSettings.configLocations = configLocations;
			ProccessSettings proccessSettings = {};

			fillStructs(argv[0], cmdLineArgs, audioSettings, renderSettings, proccessSettings);

			fpsLimit = 0;
			if (auto it = cmdLineArgs.find("fpsLimit"); it != cmdLineArgs.end())
				fpsLimit = calculate<size_t>(it->second);
			else
				WARN_UNDEFINED(fpsLimit);

			std::clog << "Initialising audio.\n";
			audioSampler.start(audioSettings);
			std::clog << "Initialising renderer.\n";
			renderer.init(renderSettings);
			proccess.init(proccessSettings);

			audioData.allocate(audioSettings.channels * audioSettings.bufferSize,
			                   audioSettings.bufferSize / 2);

			auto initEnd = std::chrono::high_resolution_clock::now();
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

		size_t fpsLimit;

		void mainLoop() {
			int numFrames = 0;
			std::chrono::microseconds targetFrameTime(1000000 / (fpsLimit ? fpsLimit : 89));
			auto lastFrame = std::chrono::high_resolution_clock::now();
			auto lastUpdate = std::chrono::steady_clock::now();

			while (audioSampler.running()) {
				if (audioSampler.modified()) {
					lastFrame = std::chrono::high_resolution_clock::now();
					audioSampler.copyData(audioData);
					proccess.proccessSignal(audioData);
					if (!renderer.drawFrame(audioData)) break;
					++numFrames;
					std::this_thread::sleep_until(lastFrame + targetFrameTime);
				}

				auto currentTime = std::chrono::steady_clock::now();
				if (std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastUpdate)
				        .count() >= 1) {
					std::clog << "FPS: " << std::setw(3) << std::right << numFrames
					          << " | UPS: " << std::setw(3) << std::right << audioSampler.ups()
					          << std::endl;
					numFrames = 0;
					lastUpdate = currentTime;
					if (!fpsLimit)
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
		                        const std::unordered_map<std::string, std::string>& configSettings,
		                        AudioSettings& audioSettings, RenderSettings& renderSettings,
		                        ProccessSettings& proccessSettings) {
			float trebleCut = 0.09f;
			if (const auto confSetting = configSettings.find("trebleCut");
			    confSetting != configSettings.end())
				trebleCut = calculate<float>(confSetting->second);
			else
				WARN_UNDEFINED(trebleCut);

			Device smoothingDevice = Device::gpu;
			if (const auto confSetting = configSettings.find("smoothingDevice");
			    confSetting != configSettings.end()) {
				if (confSetting->second == "CPU")
					smoothingDevice = Device::cpu;
				else if (confSetting->second == "GPU")
					smoothingDevice = Device::gpu;
				else
					std::cerr << LOCATION "Smoothing device set to an invalid value!\n";
			} else {
				WARN_UNDEFINED(smoothingDevice);
			}

			if (const auto confSetting = configSettings.find("channels");
			    confSetting != configSettings.end())
				audioSettings.channels = calculate<int>(confSetting->second);
			else
				WARN_UNDEFINED(channels);

			if (const auto confSetting = configSettings.find("sampleSize");
			    confSetting != configSettings.end())
				audioSettings.sampleSize = calculate<size_t>(confSetting->second);
			else
				WARN_UNDEFINED(sampleSize);

			if (const auto confSetting = configSettings.find("bufferSize");
			    confSetting != configSettings.end())
				audioSettings.bufferSize = calculate<size_t>(confSetting->second);
			else
				WARN_UNDEFINED(bufferSize);

			if (const auto confSetting = configSettings.find("sampleRate");
			    confSetting != configSettings.end())
				audioSettings.sampleRate = calculate<int>(confSetting->second);
			else
				WARN_UNDEFINED(sampleRate);

			if (const auto confSetting = configSettings.find("sinkName");
			    confSetting != configSettings.end()) {
				audioSettings.sinkName = confSetting->second;
				if (audioSettings.sinkName == "auto") audioSettings.sinkName.clear();
			} else {
				WARN_UNDEFINED(sinkName);
			}

			if (const auto confSetting = configSettings.find("modules");
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
				WARN_UNDEFINED(modules);
			}

			if (const auto confSetting = configSettings.find("backgroundImage");
			    confSetting != configSettings.end()) {
				renderSettings.backgroundImage = confSetting->second;
				if (renderSettings.backgroundImage == "none")
					renderSettings.backgroundImage.clear();
			} else {
				WARN_UNDEFINED(backgroundImage)
			}

			if (const auto confSetting = configSettings.find("width");
			    confSetting != configSettings.end())
				renderSettings.width = calculate<int>(confSetting->second);
			else
				WARN_UNDEFINED(width);

			if (const auto confSetting = configSettings.find("height");
			    confSetting != configSettings.end())
				renderSettings.height = calculate<int>(confSetting->second);
			else
				WARN_UNDEFINED(height);

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
				WARN_UNDEFINED(transparency);
			}

			if (const auto confSetting = configSettings.find("windowTitle");
			    confSetting != configSettings.end()) {
				renderSettings.windowTitle = confSetting->second;
				if (renderSettings.windowTitle == "executable")
					renderSettings.windowTitle = execPath;
			} else {
				WARN_UNDEFINED(windowTitle);
			}

			if (const auto confSetting = configSettings.find("windowPosition");
			    confSetting != configSettings.end()) {
				std::string position = confSetting->second;
				size_t gapPosition = position.find(',');
				renderSettings.windowPosition = {
				    calculate<int>(position.substr(1, gapPosition - 1)),
				    calculate<int>(
				        position.substr(gapPosition + 1, position.size() - gapPosition - 2))};
			} else {
				WARN_UNDEFINED(windowPosition);
			}

			if (const auto confSetting = configSettings.find("decorated");
			    confSetting != configSettings.end())
				renderSettings.windowHints.decorated = (confSetting->second == "true");
			else
				WARN_UNDEFINED(decorated);

			if (const auto confSetting = configSettings.find("resizable");
			    confSetting != configSettings.end())
				renderSettings.windowHints.resizable = (confSetting->second == "true");
			else
				WARN_UNDEFINED(resizable);

			if (const auto confSetting = configSettings.find("sticky");
			    confSetting != configSettings.end())
				renderSettings.windowHints.sticky = (confSetting->second == "true");
			else
				WARN_UNDEFINED(sticky);

			if (const auto confSetting = configSettings.find("windowType");
			    confSetting != configSettings.end())
				renderSettings.windowType = confSetting->second;
			else
				WARN_UNDEFINED(windowType);

			float smoothingLevel = 16.0f;
			if (const auto confSetting = configSettings.find("smoothingLevel");
			    confSetting != configSettings.end()) {
				smoothingLevel = calculate<float>(confSetting->second);
				renderSettings.smoothingLevel = smoothingLevel;
			} else {
				WARN_UNDEFINED(smoothingLevel);
			}

			renderSettings.audioSize = (audioSettings.bufferSize / 2) * (1.f - trebleCut);
			switch (smoothingDevice) {
				case Device::gpu:
					smoothingLevel = 0.f;
					break;
				case Device::cpu:
					renderSettings.smoothingLevel = 0.f;
					break;
			}

			if (const auto confSetting = configSettings.find("physicalDevice");
			    confSetting != configSettings.end()) {
				if (confSetting->second != "auto")
					renderSettings.physicalDevice.value() = calculate<int>(confSetting->second);
			} else {
				WARN_UNDEFINED(physicalDevice);
			}

			proccessSettings.channels = audioSettings.channels;
			proccessSettings.size = audioSettings.bufferSize;
			proccessSettings.smoothingLevel = smoothingLevel;

			if (const auto confSetting = configSettings.find("amplitude");
			    confSetting != configSettings.end())
				proccessSettings.amplitude = calculate<float>(confSetting->second);
			else
				WARN_UNDEFINED(amplitude);
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

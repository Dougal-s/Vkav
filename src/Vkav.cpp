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
#include "Process.hpp"
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

			cmdLineArgs.merge(readConfigFile(configFilePath));

			AudioSampler::Settings audioSettings = {};
			Renderer::Settings renderSettings = {};
			renderSettings.moduleLocations = configLocations;
			Process::Settings processSettings = {};

			fillStructs(cmdLineArgs, audioSettings, renderSettings, processSettings);

			fpsLimit = 0;
			if (auto it = cmdLineArgs.find("fpsLimit"); it != cmdLineArgs.end())
				fpsLimit = calculate<size_t>(it->second);
			else
				WARN_UNDEFINED(fpsLimit);

			std::clog << "Initialising audio" << std::endl;
			audioSampler = AudioSampler(audioSettings);
			std::clog << "Initialising renderer" << std::endl;
			renderer = Renderer(renderSettings);
			process = Process(processSettings);

			audioData.allocate(audioSettings.channels, audioSettings.bufferSize);

			auto initEnd = std::chrono::high_resolution_clock::now();
			std::clog << "Initialisation took: "
			          << std::chrono::duration_cast<std::chrono::milliseconds>(initEnd - initStart)
			                 .count()
			          << " milliseconds" << std::endl;
		}

		void run() {
			int numFrames = 0;
			std::chrono::microseconds targetFrameTime(1000000 / (fpsLimit ? fpsLimit : 89));
			auto lastFrame = std::chrono::high_resolution_clock::now();
			auto lastUpdate = std::chrono::steady_clock::now();

			while (audioSampler.running()) {
				if (audioSampler.modified()) {
					lastFrame = std::chrono::high_resolution_clock::now();
					audioSampler.copyData(audioData);
					process.processSignal(audioData);
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

	private:
		AudioData audioData;

		AudioSampler audioSampler;
		Renderer renderer;
		Process process;

		size_t fpsLimit;

		static void fillStructs(const std::unordered_map<std::string, std::string>& settings,
		                        AudioSampler::Settings& audioSettings,
		                        Renderer::Settings& renderSettings,
		                        Process::Settings& processSettings) {
			float trebleCut = 0.09f;
			if (const auto setting = settings.find("trebleCut"); setting != settings.end())
				trebleCut = calculate<float>(setting->second);
			else
				WARN_UNDEFINED(trebleCut);

			Device smoothingDevice = Device::gpu;
			if (const auto setting = settings.find("smoothingDevice"); setting != settings.end()) {
				if (setting->second == "CPU")
					smoothingDevice = Device::cpu;
				else if (setting->second == "GPU")
					smoothingDevice = Device::gpu;
				else
					std::cerr << LOCATION "Smoothing device set to an invalid value!\n";
			} else {
				WARN_UNDEFINED(smoothingDevice);
			}

			if (const auto setting = settings.find("channels"); setting != settings.end())
				audioSettings.channels = calculate<int>(setting->second);
			else
				WARN_UNDEFINED(channels);

			if (const auto setting = settings.find("sampleSize"); setting != settings.end())
				audioSettings.sampleSize = calculate<size_t>(setting->second);
			else
				WARN_UNDEFINED(sampleSize);

			if (const auto setting = settings.find("bufferSize"); setting != settings.end())
				audioSettings.bufferSize = calculate<size_t>(setting->second);
			else
				WARN_UNDEFINED(bufferSize);

			if (const auto setting = settings.find("sampleRate"); setting != settings.end())
				audioSettings.sampleRate = calculate<int>(setting->second);
			else
				WARN_UNDEFINED(sampleRate);

			if (const auto setting = settings.find("sinkName"); setting != settings.end()) {
				if (setting->second != "auto")
					audioSettings.sinkName = parseAsString(setting->second);
			} else {
				WARN_UNDEFINED(sinkName);
			}

			if (const auto setting = settings.find("modules"); setting != settings.end()) {
				auto modules = parseAsArray(setting->second);
				renderSettings.modules.clear();
				renderSettings.modules.reserve(modules.size());
				for (auto module : modules) renderSettings.modules.push_back(parseAsString(module));
			} else {
				WARN_UNDEFINED(modules);
			}

			if (const auto setting = settings.find("backgroundImage"); setting != settings.end()) {
				if (setting->second != "none")
					renderSettings.backgroundImage = parseAsString(setting->second);
			} else {
				WARN_UNDEFINED(backgroundImage)
			}

			if (const auto setting = settings.find("width"); setting != settings.end())
				renderSettings.window.width = calculate<int>(setting->second);
			else
				WARN_UNDEFINED(width);

			if (const auto setting = settings.find("height"); setting != settings.end())
				renderSettings.window.height = calculate<int>(setting->second);
			else
				WARN_UNDEFINED(height);

			if (const auto setting = settings.find("windowPosition"); setting != settings.end()) {
				if (setting->second != "auto") {
					auto [x, y] = parseAsPair(setting->second);
					renderSettings.window.position = std::make_pair(calculate<int>(std::string(x)),
					                                                calculate<int>(std::string(y)));
				}
			} else {
				WARN_UNDEFINED(windowPosition);
			}

			if (const auto setting = settings.find("transparency"); setting != settings.end()) {
				std::string transparency = setting->second;
				if (transparency == "Vulkan")
					renderSettings.window.transparency =
					    Renderer::Settings::Window::Transparency::vulkan;
				else if (transparency == "Native")
					renderSettings.window.transparency =
					    Renderer::Settings::Window::Transparency::native;
				else if (transparency == "Opaque")
					renderSettings.window.transparency =
					    Renderer::Settings::Window::Transparency::opaque;
				else
					std::cerr << LOCATION "Transparency set to an invalid value!\n";
			} else {
				WARN_UNDEFINED(transparency);
			}

			if (const auto setting = settings.find("windowTitle"); setting != settings.end()) {
				renderSettings.window.title = parseAsString(setting->second);
			} else {
				WARN_UNDEFINED(windowTitle);
			}

			if (const auto setting = settings.find("decorated"); setting != settings.end())
				renderSettings.window.hints.decorated = (setting->second == "true");
			else
				WARN_UNDEFINED(decorated);

			if (const auto setting = settings.find("resizable"); setting != settings.end())
				renderSettings.window.hints.resizable = (setting->second == "true");
			else
				WARN_UNDEFINED(resizable);

			if (const auto setting = settings.find("sticky"); setting != settings.end())
				renderSettings.window.hints.sticky = (setting->second == "true");
			else
				WARN_UNDEFINED(sticky);

			if (const auto setting = settings.find("windowType"); setting != settings.end())
				renderSettings.window.type = setting->second;
			else
				WARN_UNDEFINED(windowType);

			float smoothingLevel = 16.0f;
			if (const auto setting = settings.find("smoothingLevel"); setting != settings.end()) {
				smoothingLevel = calculate<float>(setting->second);
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

			if (const auto setting = settings.find("physicalDevice"); setting != settings.end()) {
				if (setting->second != "auto")
					renderSettings.physicalDevice.value() = calculate<int>(setting->second);
			} else {
				WARN_UNDEFINED(physicalDevice);
			}

			processSettings.channels = audioSettings.channels;
			processSettings.size = audioSettings.bufferSize;
			processSettings.smoothingLevel = smoothingLevel;

			if (const auto setting = settings.find("amplitude"); setting != settings.end())
				processSettings.amplitude = calculate<float>(setting->second);
			else
				WARN_UNDEFINED(amplitude);
		}
	};
}  // namespace

int main(int argc, const char* argv[]) {
	std::ios_base::sync_with_stdio(false);
	try {
		Vkav app(argc, argv);
		app.run();
	} catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

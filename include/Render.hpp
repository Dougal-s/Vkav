#pragma once
#ifndef RENDER_HPP
#define RENDER_HPP

#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>
struct AudioData;

class Renderer {
public:
	struct Settings {
		struct Window {
			enum class Transparency { vulkan, native, opaque };

			uint32_t width = 800;
			uint32_t height = 800;
			std::optional<std::pair<int, int>> position;

			Transparency transparency = Transparency::opaque;

			std::string title = "Vkav";
			struct Hints {
				bool decorated = true;
				bool resizable = true;
				bool sticky = false;
			} hints;
			std::string type;
		};

		Window window;

		size_t audioSize;
		float smoothingLevel = 16.f;
		std::vector<std::filesystem::path> moduleLocations;
		std::vector<std::filesystem::path> modules = {1, "bars"};
		std::filesystem::path backgroundImage;

		std::optional<uint32_t> physicalDevice;

		bool vsync;
	};

	Renderer() = default;
	Renderer(const Settings& renderSettings);
	~Renderer();

	Renderer& operator=(Renderer&& other) noexcept;

	bool drawFrame(const AudioData& audioData);
private:
	class RendererImpl;
	RendererImpl* rendererImpl = nullptr;
};

#endif

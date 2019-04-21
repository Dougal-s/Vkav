#pragma once
#ifndef RENDER_HPP
#define RENDER_HPP

#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Data.hpp"

enum TransparencyType {
	VULKAN,
	NATIVE,
	OPAQUE
};

struct RenderSettings {
	uint32_t width  = 800;
	uint32_t height = 800;
	std::optional<std::pair<int, int>> windowPosition;

	TransparencyType transparency = OPAQUE;

	std::string windowTitle = "Vkav";

	struct WindowHints {
		bool decorated = true;
		bool resizable = true;
	} windowHints;

	size_t audioSize;
	float smoothingLevel = 16.f;
	std::vector<std::filesystem::path> shaderDirectories = std::vector<std::filesystem::path>(1, "shaders/bars");
	std::filesystem::path backgroundImage;

	std::optional<uint32_t> physicalDevice;
};

class Renderer {
public:
	void init(const RenderSettings& renderSettings);

	bool drawFrame(const AudioData& audioData);

	void cleanup();

private:
	class RendererImpl;
	RendererImpl* rendererImpl;
};

#endif

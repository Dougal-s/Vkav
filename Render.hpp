#pragma once
#ifndef RENDER_HPP
#define RENDER_HPP

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

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

struct QueueFamilyIndices {
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;

	bool isComplete() {
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};

struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

union SpecializationConstant {
	int32_t intVal;
	float floatVal;
};

struct SpecializationConstants {
	std::vector<SpecializationConstant> data;
	std::vector<VkSpecializationMapEntry> specializationInfo;
};

struct GraphicsPipeline {
	VkPipeline graphicsPipeline;
	SpecializationConstants specializationConstants;

	VkShaderModule fragShaderModule;
	// Name of the fragment shader function to call
	std::string moduleName;
};

class Renderer {
public:
	void init(const RenderSettings& renderSettings);

	bool drawFrame(const AudioData& audioData);

	void cleanup();

private:

	// Variables

	RenderSettings settings;

	GLFWwindow* window;

	VkInstance instance;
	VkDebugUtilsMessengerEXT debugMessenger;

	VkSurfaceKHR surface;

	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device;

	VkQueue graphicsQueue;
	VkQueue presentQueue;

	VkSwapchainKHR swapChain;
	std::vector<VkImage> swapChainImages;
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;

	std::vector<VkImageView> swapChainImageViews;
	std::vector<VkFramebuffer> swapChainFramebuffers;

	VkRenderPass renderPass;
	VkDescriptorSetLayout descriptorSetLayout;
	VkPipelineLayout pipelineLayout;
	std::vector<GraphicsPipeline> graphicsPipelines;

	VkCommandPool commandPool;
	std::vector<VkCommandBuffer> commandBuffers;

	std::vector<VkBuffer> volumeBuffers;
	std::vector<VkDeviceMemory> volumeBufferMemory;

	std::vector<VkBuffer> lAudioBuffers;
	std::vector<VkDeviceMemory> lAudioBufferMemory;
	std::vector<VkBufferView> lAudioBufferViews;
	std::vector<VkBuffer> rAudioBuffers;
	std::vector<VkDeviceMemory> rAudioBufferMemory;
	std::vector<VkBufferView> rAudioBufferViews;

	VkImage backgroundImage;
	VkDeviceMemory backgroundImageMemory;
	VkImageView backgroundImageView;
	VkSampler backgroundImageSampler;

	VkDescriptorPool descriptorPool;
	std::vector<VkDescriptorSet> descriptorSets;

	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::vector<VkFence> inFlightFences;
	size_t currentFrame = 0;

	bool framebufferResized = false;

	// Member functions

	void initWindow();

	void initVulkan();

	void createInstance();

	std::vector<const char*> getRequiredExtensions();

	bool checkRequiredExtensionsPresent(const std::vector<const char*>& extensions);

	bool checkValidationLayerSupport();

	void setupDebugCallback();

	void createSurface();

	void pickPhysicalDevice();

	bool isDeviceSuitable(VkPhysicalDevice device);

	bool checkDeviceExtensionSupport(VkPhysicalDevice device);

	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

	void createLogicalDevice();

	void createSwapchain();

	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);

	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

	void createImageViews();

	void destroyGraphicsPipelines();

	void prepareGraphicsPipelineCreation();

	void createGraphicsPipelines();

	VkShaderModule createShaderModule(const std::vector<char>& shaderCode);

	void createRenderPass();

	void createFramebuffers();

	void createCommandPool();

	void createCommandBuffers();

	void createSyncObjects();

	void cleanupSwapChain();

	void recreateSwapChain();

	void createBackgroundImage();

	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

	void createBuffer(
		VkDeviceSize size,
		VkBufferUsageFlags usage,
		VkMemoryPropertyFlags properties,
		VkBuffer& buffer,
		VkDeviceMemory& bufferMemory
	);

	void createBufferView(VkDeviceSize size, VkFormat format, VkBuffer buffer, VkBufferView& bufferView);

	void createImage(
		uint32_t width,
		uint32_t height,
		VkImageType imageType,
		VkFormat format,
		VkImageTiling tiling,
		VkImageUsageFlags usage,
		VkMemoryPropertyFlags properties,
		VkImage& image,
		VkDeviceMemory& imageMemory
	);

	VkCommandBuffer beginSingleTimeCommands();

	void endSingleTimeCommands(VkCommandBuffer commandBuffer);

	void transitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout);

	void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

	void createBackgroundImageView();

	VkImageView createImageView(VkImage image, VkFormat format);

	void createBackgroundImageSampler();

	void createDescriptorSetLayout();

	void createAudioBuffers();

	void updateAudioBuffers(const AudioData& audioData, uint32_t currentFrame);

	void updateAudioImages(const AudioData& audioData, uint32_t currentFrame);

	void createDescriptorPool();

	void createDescriptorSets();

	// Static member functions

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* userData
	);

	static std::vector<char> readFile(const std::filesystem::path& filePath);

	static SpecializationConstants readSpecializationConstants(const std::filesystem::path& configFilePath, std::string& moduleName);

	static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

};

#endif

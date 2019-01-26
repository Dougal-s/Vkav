#ifndef RENDER_HPP
#define RENDER_HPP

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <optional> // std::optional<>
#include <string>   // std::string<>
#include <utility>  // std::pair<>
#include <vector>   // std::vector<>

enum TransparencyType {
	VULKAN,
	NATIVE,
	OPAQUE
};

struct RendererSettings {
	uint32_t width  = 800;
	uint32_t height = 800;
	std::optional<std::pair<int, int>> windowPosition;

	TransparencyType transparency = OPAQUE;

	std::string windowTitle = "Vkav";

	struct WindowHints {
		bool decorated = true;
		bool resizable = true;
	} windowHints;

	uint32_t audioSize;
	float smoothingLevel = 16.0f;
	std::string shaderPath = "shaders/bars/frag.spv";

	std::optional<uint32_t> physicalDevice;
};

// Data Structs

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

class Renderer {
public:
	void init(const RendererSettings& rendererSettings);

	bool drawFrame(const std::vector<float>& lBuffer, const std::vector<float>& rBuffer);

	void cleanup();

private:

	// Variables

	GLFWwindow* window;

	VkInstance instance;
	VkDebugUtilsMessengerEXT callback;
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
	VkPipeline graphicsPipeline;

	VkCommandPool commandPool;

	VkBuffer lStagingBuffer;
	VkBuffer rStagingBuffer;
	void* lStagingBufferData;
	void* rStagingBufferData;
	VkDeviceMemory lStagingBufferMemory;
	VkDeviceMemory rStagingBufferMemory;

	std::vector<VkImage> lAudioImages;
	std::vector<VkImage> rAudioImages;
	std::vector<VkDeviceMemory> lAudioImageMemory;
	std::vector<VkDeviceMemory> rAudioImageMemory;
	std::vector<VkImageView> lAudioImageViews;
	std::vector<VkImageView> rAudioImageViews;
	VkSampler audioImageSampler;

	VkDescriptorPool descriptorPool;
	std::vector<VkDescriptorSet> descriptorSets;

	std::vector<VkCommandBuffer> commandBuffers;
	std::vector<VkCommandBuffer> dataTransferCommandBuffers;

	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::vector<VkFence> inFlightFences;

	std::vector<VkSemaphore> bufferToImageCopyCompleteSemaphore;
	VkFence audioBuffersAvailableFence;
	size_t currentFrame = 0;

	bool windowIconified = false;

	RendererSettings settings;

	// Functions

	void initWindow();

	void initVulkan();

	void createInstance();

	bool checkRequiredExtensionsPresent(const std::vector<const char*>& requiredExt, const std::vector<VkExtensionProperties>& availableExt);

	bool checkValidationLayerSupport();

	std::vector<const char*> getRequiredExtensions();

	void setupDebugCallback();

	void pickPhysicalDevice();

	int rateDeviceSuitability(VkPhysicalDevice device);

	bool checkDeviceExtensionSupport(VkPhysicalDevice device);

	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

	void createLogicalDevice();

	void createSurface();

	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);

	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

	void createSwapChain();

	void createSwapChainImageViews();

	void createGraphicsPipeline();

	VkShaderModule createShaderModule(const std::vector<char>& code);

	void createRenderPass();

	void createFramebuffers();

	void createCommandPool();

	void createCommandBuffers();

	void transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout);

	void createSyncObjects();

	void recreateSwapChain();

	void cleanupSwapChain();

	void createAudioBuffer();

	void createAudioImages();

	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);

	void createImage(uint32_t width, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);

	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

	void createAudioImageViews();

	VkImageView createImageView(VkImage image, VkFormat format, VkImageViewType viewType);

	void createAudioImageSampler();

	void createDescriptorSetLayout();

	void createDescriptorPool();

	void createDescriptorSets();

	// Static member functions

	static void windowIconifyCallback(GLFWwindow* window, int iconified);

	static std::vector<char> readFile(const std::string& filename);

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData
	);
};


#endif

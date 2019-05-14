#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include "Data.hpp"
#include "Image.hpp"
#include "Render.hpp"

// Miscellaneous variables

namespace {
	constexpr int MAX_FRAMES_IN_FLIGHT = 2;

	constexpr const char* VERTEX_SHADER_PATH = "./shaders/vert.spv";

	const std::vector<const char*> validationLayers = {
	    "VK_LAYER_LUNARG_standard_validation"};

	const std::vector<const char*> deviceExtensions = {
	    VK_KHR_SWAPCHAIN_EXTENSION_NAME};

#ifdef NDEBUG
	constexpr bool enableValidationLayers = false;
#else
	constexpr bool enableValidationLayers = true;
#endif

	VkResult createDebugUtilsMessengerEXT(
	    VkInstance instance,
	    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
	    const VkAllocationCallbacks* pAllocator,
	    VkDebugUtilsMessengerEXT* pDebugMessenger) {
		auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
		    vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
		if (func == nullptr) return VK_ERROR_EXTENSION_NOT_PRESENT;
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}

	void DestroyDebugUtilsMessengerEXT(
	    VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
	    const VkAllocationCallbacks* pAllocator) {
		auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
		    vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
		if (func == nullptr) return;
		func(instance, debugMessenger, pAllocator);
	}

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

	typedef std::variant<uint32_t, int32_t, float> SpecializationConstant;

	struct SpecializationConstants {
		std::vector<SpecializationConstant> data;
		std::vector<VkSpecializationMapEntry> specializationInfo;
	};

	struct GraphicsPipeline {
		VkPipeline graphicsPipeline;
		SpecializationConstants specializationConstants;

		VkShaderModule fragShaderModule;
		// Name of the fragment shader function to call
		std::string moduleName = "main";

		VkShaderModule vertShaderModule;
	};
}  // namespace

class Renderer::RendererImpl {
public:
	RendererImpl(const RenderSettings& renderSettings) {
		settings = renderSettings;

		initWindow();
		initVulkan();
	}

	bool drawFrame(const AudioData& audioData) {
		if (glfwWindowShouldClose(window)) return false;
		glfwPollEvents();

		vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE,
		                std::numeric_limits<uint64_t>::max());

		uint32_t imageIndex;
		VkResult result = vkAcquireNextImageKHR(
		    device, swapChain, std::numeric_limits<uint64_t>::max(),
		    imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE,
		    &imageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			recreateSwapChain();
			return true;
		} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
			throw std::runtime_error(__FILE__
			                         ": failed to acquire swap chain image!");
		}

		updateAudioBuffers(audioData, imageIndex);

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
		VkPipelineStageFlags waitStages[] = {
		    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffers[imageIndex];

		VkSemaphore signalSemaphores[] = {
		    renderFinishedSemaphores[currentFrame]};
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		vkResetFences(device, 1, &inFlightFences[currentFrame]);

		if (vkQueueSubmit(graphicsQueue, 1, &submitInfo,
		                  inFlightFences[currentFrame]) != VK_SUCCESS)
			throw std::runtime_error(__FILE__
			                         ": failed to submit draw command buffer!");

		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapChains[] = {swapChain};
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;

		presentInfo.pImageIndices = &imageIndex;

		result = vkQueuePresentKHR(presentQueue, &presentInfo);

		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
		    framebufferResized) {
			framebufferResized = false;
			recreateSwapChain();
		} else if (result != VK_SUCCESS) {
			throw std::runtime_error(__FILE__
			                         ": failed to present swap chain image!");
		}

		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

		return true;
	}

	~RendererImpl() {
		vkDeviceWaitIdle(device);

		cleanupSwapChain();

		vkDestroyDescriptorPool(device, descriptorPool, nullptr);

		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

		for (size_t i = 0; i < volumeBuffers.size(); ++i) {
			vkDestroyBuffer(device, volumeBuffers[i], nullptr);
			vkFreeMemory(device, volumeBufferMemory[i], nullptr);
			vkDestroyBufferView(device, lAudioBufferViews[i], nullptr);
			vkDestroyBuffer(device, lAudioBuffers[i], nullptr);
			vkFreeMemory(device, lAudioBufferMemory[i], nullptr);
			vkDestroyBufferView(device, rAudioBufferViews[i], nullptr);
			vkDestroyBuffer(device, rAudioBuffers[i], nullptr);
			vkFreeMemory(device, rAudioBufferMemory[i], nullptr);
		}

		destroyGraphicsPipelines();

		vkDestroySampler(device, backgroundImageSampler, nullptr);
		vkDestroyImageView(device, backgroundImageView, nullptr);

		vkDestroyImage(device, backgroundImage, nullptr);
		vkFreeMemory(device, backgroundImageMemory, nullptr);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
			vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
			vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
			vkDestroyFence(device, inFlightFences[i], nullptr);
		}

		vkDestroyCommandPool(device, commandPool, nullptr);

		vkDestroyDevice(device, nullptr);

		if (enableValidationLayers)
			DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);

		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);

		glfwDestroyWindow(window);

		glfwTerminate();
	}

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

	void initWindow() {
		glfwInit();

		if (!glfwVulkanSupported())
			throw std::runtime_error(
			    __FILE__ ": vulkan not supported by the current environment!");

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		glfwWindowHint(GLFW_DECORATED,
		               settings.windowHints.decorated ? GLFW_TRUE : GLFW_FALSE);
		glfwWindowHint(GLFW_RESIZABLE,
		               settings.windowHints.resizable ? GLFW_TRUE : GLFW_FALSE);

#ifdef GLFW_TRANSPARENT_FRAMEBUFFER
		glfwWindowHint(
		    GLFW_TRANSPARENT_FRAMEBUFFER,
		    settings.transparency == NATIVE ? GLFW_TRUE : GLFW_FALSE);
#else
		if (settings.transparency == NATIVE) {
			std::cerr
			    << "Native transaprency unsupported by current configuration!"
			    << std::endl;
			settings.transparency = OPAQUE;
		}
#endif

		window =
		    glfwCreateWindow(settings.width, settings.height,
		                     settings.windowTitle.c_str(), nullptr, nullptr);
		glfwSetWindowUserPointer(window, this);
		glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);

		if (settings.windowPosition)
			glfwSetWindowPos(window, settings.windowPosition.value().first,
			                 settings.windowPosition.value().second);
	}

	void initVulkan() {
		createInstance();
		setupDebugCallback();
		createSurface();
		pickPhysicalDevice();
		createLogicalDevice();
		createSwapchain();
		createImageViews();
		createRenderPass();
		createDescriptorSetLayout();
		prepareGraphicsPipelineCreation();
		createGraphicsPipelines();
		createFramebuffers();
		createCommandPool();
		createAudioBuffers();
		createBackgroundImage();
		createBackgroundImageView();
		createBackgroundImageSampler();
		createDescriptorPool();
		createDescriptorSets();
		createCommandBuffers();
		createSyncObjects();
	}

	void createInstance() {
		if (enableValidationLayers && !checkValidationLayerSupport())
			throw std::runtime_error(__FILE__
			                         ": validation layers unavailable!");

		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Vkav";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 1, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo instanceInfo = {};
		instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		instanceInfo.pApplicationInfo = &appInfo;

		auto extensions = getRequiredExtensions();
		if (!checkRequiredExtensionsPresent(extensions))
			throw std::runtime_error(__FILE__
			                         ": missing required vulkan extension!");

		instanceInfo.enabledExtensionCount =
		    static_cast<uint32_t>(extensions.size());
		instanceInfo.ppEnabledExtensionNames = extensions.data();

		if (enableValidationLayers) {
			instanceInfo.enabledLayerCount =
			    static_cast<uint32_t>(validationLayers.size());
			instanceInfo.ppEnabledLayerNames = validationLayers.data();
		} else {
			instanceInfo.enabledLayerCount = 0;
		}

		if (vkCreateInstance(&instanceInfo, nullptr, &instance) != VK_SUCCESS)
			throw std::runtime_error(__FILE__
			                         ": failed to create a vulkan instance!");
	}

	std::vector<const char*> getRequiredExtensions() {
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::vector<const char*> extensions(
		    glfwExtensions, glfwExtensions + glfwExtensionCount);

		if (enableValidationLayers) {
			extensions.reserve(extensions.size() + 1);
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		return extensions;
	}

	bool checkRequiredExtensionsPresent(
	    const std::vector<const char*>& extensions) {
		uint32_t availableExtensionCount = 0;
		vkEnumerateInstanceExtensionProperties(
		    nullptr, &availableExtensionCount, nullptr);
		std::vector<VkExtensionProperties> availableExtensions(
		    availableExtensionCount);
		vkEnumerateInstanceExtensionProperties(
		    nullptr, &availableExtensionCount, availableExtensions.data());

		std::set<std::string> requiredExtensions(extensions.begin(),
		                                         extensions.end());

		for (const auto& extension : availableExtensions)
			requiredExtensions.erase(extension.extensionName);

		return requiredExtensions.empty();
	}

	bool checkValidationLayerSupport() {
		uint32_t availableLayerCount = 0;
		vkEnumerateInstanceLayerProperties(&availableLayerCount, nullptr);
		std::vector<VkLayerProperties> availableLayers(availableLayerCount);
		vkEnumerateInstanceLayerProperties(&availableLayerCount,
		                                   availableLayers.data());

		std::set<std::string> requiredLayers(validationLayers.begin(),
		                                     validationLayers.end());

		for (const auto& layer : availableLayers)
			requiredLayers.erase(layer.layerName);

		return requiredLayers.empty();
	}

	void setupDebugCallback() {
		if (!enableValidationLayers) return;

		VkDebugUtilsMessengerCreateInfoEXT messengerInfo = {};
		messengerInfo.sType =
		    VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		messengerInfo.messageSeverity =
		    VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
		    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		messengerInfo.messageType =
		    VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		messengerInfo.pfnUserCallback = debugCallback;

		if (createDebugUtilsMessengerEXT(instance, &messengerInfo, nullptr,
		                                 &debugMessenger) != VK_SUCCESS)
			throw std::runtime_error(__FILE__
			                         ": failed to create debug messenger!");
	}

	void createSurface() {
		if (glfwCreateWindowSurface(instance, window, nullptr, &surface) !=
		    VK_SUCCESS)
			throw std::runtime_error(__FILE__
			                         ": failed to create window surface!");
	}

	void pickPhysicalDevice() {
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
		if (deviceCount == 0)
			throw std::runtime_error(
			    __FILE__ ": failed to find GPUs with Vulkan support!");

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

		if (settings.physicalDevice) {
			if (settings.physicalDevice.value() >= deviceCount ||
			    !isDeviceSuitable(devices[settings.physicalDevice.value()]))
				throw std::runtime_error(__FILE__ ": invalid GPU selected!");

			physicalDevice = devices[settings.physicalDevice.value()];
			return;
		}

		for (const auto& device : devices) {
			if (isDeviceSuitable(device)) {
				physicalDevice = device;
				break;
			}
		}

		if (physicalDevice == VK_NULL_HANDLE)
			throw std::runtime_error(__FILE__
			                         ": failed to find a suitable GPU!");
	}

	bool isDeviceSuitable(VkPhysicalDevice device) {
		QueueFamilyIndices indices = findQueueFamilies(device);

		bool extensionsSupported = checkDeviceExtensionSupport(device);

		bool swapChainAdequate = false;
		if (extensionsSupported) {
			SwapChainSupportDetails swapChainDetails =
			    querySwapChainSupport(device);
			swapChainAdequate = !swapChainDetails.formats.empty() &&
			                    !swapChainDetails.presentModes.empty();
		}

		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(device, &deviceProperties);

		// Size of audio buffers + volume <= maximum capacity
		bool uniformBufferSizeAdequate =
		    2 * (settings.audioSize + 1) * sizeof(float) <=
		    deviceProperties.limits.maxUniformBufferRange;

		return indices.isComplete() && extensionsSupported &&
		       swapChainAdequate && uniformBufferSizeAdequate;
	}

	bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
		uint32_t availableExtensionCount = 0;
		vkEnumerateDeviceExtensionProperties(device, nullptr,
		                                     &availableExtensionCount, nullptr);
		std::vector<VkExtensionProperties> availableExtensions(
		    availableExtensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr,
		                                     &availableExtensionCount,
		                                     availableExtensions.data());

		std::set<std::string> requiredExtensions(deviceExtensions.begin(),
		                                         deviceExtensions.end());
		for (const auto& availableExtension : availableExtensions)
			requiredExtensions.erase(availableExtension.extensionName);

		return requiredExtensions.empty();
	}

	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
		QueueFamilyIndices indices;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
		                                         nullptr);
		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
		                                         queueFamilies.data());

		int i = 0;
		for (const auto& queueFamily : queueFamilies) {
			if (queueFamily.queueCount <= 0) continue;

			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface,
			                                     &presentSupport);

			if (presentSupport) indices.presentFamily = i;

			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
				indices.graphicsFamily = i;

			if (indices.isComplete()) break;

			++i;
		}

		return indices;
	}

	void createLogicalDevice() {
		QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

		std::vector<VkDeviceQueueCreateInfo> queueInfos;
		std::set<uint32_t> uniqueQueueFamilies = {
		    indices.graphicsFamily.value(), indices.presentFamily.value()};

		float queuePriority = 1.f;
		for (const auto& queueFamily : uniqueQueueFamilies) {
			VkDeviceQueueCreateInfo queueInfo = {};
			queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueInfo.queueFamilyIndex = queueFamily;
			queueInfo.queueCount = 1;
			queueInfo.pQueuePriorities = &queuePriority;
			queueInfos.push_back(queueInfo);
		}

		VkPhysicalDeviceFeatures deviceFeatures = {};

		VkDeviceCreateInfo deviceInfo = {};
		deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceInfo.pQueueCreateInfos = queueInfos.data();
		deviceInfo.queueCreateInfoCount =
		    static_cast<uint32_t>(queueInfos.size());
		deviceInfo.pEnabledFeatures = &deviceFeatures;
		deviceInfo.enabledExtensionCount =
		    static_cast<uint32_t>(deviceExtensions.size());
		deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();

		if (enableValidationLayers) {
			deviceInfo.enabledLayerCount =
			    static_cast<uint32_t>(validationLayers.size());
			deviceInfo.ppEnabledLayerNames = validationLayers.data();
		} else {
			deviceInfo.enabledLayerCount = 0;
		}

		if (vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device) !=
		    VK_SUCCESS)
			throw std::runtime_error(__FILE__
			                         ": failed to create logical device!");

		vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0,
		                 &graphicsQueue);
		vkGetDeviceQueue(device, indices.presentFamily.value(), 0,
		                 &presentQueue);
	}

	void createSwapchain() {
		SwapChainSupportDetails swapChainSupport =
		    querySwapChainSupport(physicalDevice);

		VkSurfaceFormatKHR surfaceFormat =
		    chooseSwapSurfaceFormat(swapChainSupport.formats);
		VkPresentModeKHR presentMode =
		    chooseSwapPresentMode(swapChainSupport.presentModes);
		VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

		uint32_t imageCount = swapChainSupport.capabilities.minImageCount;

		if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) ++imageCount;

		if (swapChainSupport.capabilities.maxImageCount > 0 &&
		    imageCount > swapChainSupport.capabilities.maxImageCount)
			imageCount = swapChainSupport.capabilities.maxImageCount;

		VkSwapchainCreateInfoKHR swapChainInfo = {};
		swapChainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swapChainInfo.surface = surface;
		swapChainInfo.minImageCount = imageCount;
		swapChainInfo.imageFormat = surfaceFormat.format;
		swapChainInfo.imageColorSpace = surfaceFormat.colorSpace;
		swapChainInfo.imageExtent = extent;
		swapChainInfo.imageArrayLayers = 1;
		swapChainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
		uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(),
		                                 indices.presentFamily.value()};

		if (indices.graphicsFamily.value() != indices.presentFamily) {
			swapChainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			swapChainInfo.queueFamilyIndexCount = 2;
			swapChainInfo.pQueueFamilyIndices = queueFamilyIndices;
		} else {
			swapChainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			swapChainInfo.queueFamilyIndexCount = 0;
			swapChainInfo.pQueueFamilyIndices = nullptr;
		}

		swapChainInfo.preTransform =
		    swapChainSupport.capabilities.currentTransform;

		switch (settings.transparency) {
			case NATIVE:
				if (swapChainSupport.capabilities.supportedCompositeAlpha &
				    VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR) {
					swapChainInfo.compositeAlpha =
					    VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
				} else {
					std::cerr << "native transparency not supported!\n";
					swapChainInfo.compositeAlpha =
					    VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
					settings.transparency = OPAQUE;
				}
				break;
			case VULKAN:
				if (swapChainSupport.capabilities.supportedCompositeAlpha &
				    VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR) {
					swapChainInfo.compositeAlpha =
					    VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
				} else {
					std::cerr << "vulkan transparency not supported!\n";
					swapChainInfo.compositeAlpha =
					    VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
					settings.transparency = OPAQUE;
				}
				break;
			case OPAQUE:
				swapChainInfo.compositeAlpha =
				    VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
				break;
		}

		swapChainInfo.presentMode = presentMode;
		swapChainInfo.clipped = VK_TRUE;
		swapChainInfo.oldSwapchain = VK_NULL_HANDLE;

		if (vkCreateSwapchainKHR(device, &swapChainInfo, nullptr, &swapChain) !=
		    VK_SUCCESS)
			throw std::runtime_error(__FILE__ ": failed to create swapchain!");

		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
		swapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(device, swapChain, &imageCount,
		                        swapChainImages.data());

		swapChainImageFormat = surfaceFormat.format;
		swapChainExtent = extent;
	}

	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
		SwapChainSupportDetails details;
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface,
		                                          &details.capabilities);

		uint32_t formatCount = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount,
		                                     nullptr);

		if (formatCount != 0) {
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount,
			                                     details.formats.data());
		}

		uint32_t presentModeCount = 0;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface,
		                                          &presentModeCount, nullptr);

		if (formatCount != 0) {
			details.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(
			    device, surface, &presentModeCount,
			    details.presentModes.data());
		}

		return details;
	}

	VkSurfaceFormatKHR chooseSwapSurfaceFormat(
	    const std::vector<VkSurfaceFormatKHR>& availableFormats) {
		if (availableFormats.size() == 1 &&
		    availableFormats[0].format == VK_FORMAT_UNDEFINED)
			return {VK_FORMAT_B8G8R8A8_UNORM,
			        VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};

		for (const auto& availableFormat : availableFormats) {
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM &&
			    availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
				return availableFormat;
		}

		return availableFormats[0];
	}

	VkPresentModeKHR chooseSwapPresentMode(
	    const std::vector<VkPresentModeKHR>& availablePresentModes) {
		VkPresentModeKHR bestMode = VK_PRESENT_MODE_FIFO_KHR;

		for (const auto& availablePresentMode : availablePresentModes) {
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
				return availablePresentMode;
			else if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR)
				bestMode = availablePresentMode;
		}

		return bestMode;
	}

	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
		if (capabilities.currentExtent.width !=
		    std::numeric_limits<uint32_t>::max()) {
			return capabilities.currentExtent;
		} else {
			int width, height;
			glfwGetFramebufferSize(window, &width, &height);

			VkExtent2D extent = {static_cast<uint32_t>(width),
			                     static_cast<uint32_t>(height)};

			extent.width =
			    std::clamp(extent.width, capabilities.minImageExtent.width,
			               capabilities.maxImageExtent.width);
			extent.height =
			    std::clamp(extent.height, capabilities.minImageExtent.height,
			               capabilities.maxImageExtent.height);

			return extent;
		}
	}

	void createImageViews() {
		swapChainImageViews.resize(swapChainImages.size());

		for (size_t i = 0; i < swapChainImages.size(); ++i)
			swapChainImageViews[i] =
			    createImageView(swapChainImages[i], swapChainImageFormat);
	}

	void destroyGraphicsPipelines() {
		for (auto& graphicsPipeline : graphicsPipelines) {
			vkDestroyShaderModule(device, graphicsPipeline.fragShaderModule,
			                      nullptr);
			vkDestroyShaderModule(device, graphicsPipeline.vertShaderModule,
			                      nullptr);
		}
	}

	void prepareGraphicsPipelineCreation() {
		graphicsPipelines.resize(settings.shaderDirectories.size());

		for (uint32_t i = 0; i < graphicsPipelines.size(); ++i) {
			std::filesystem::path vertexShaderPath =
			    settings.shaderDirectories[i];
			vertexShaderPath /= "vert.spv";
			if (!std::filesystem::exists(vertexShaderPath))
				vertexShaderPath = VERTEX_SHADER_PATH;

			auto vertShaderCode = readFile(vertexShaderPath);
			graphicsPipelines[i].vertShaderModule =
			    createShaderModule(vertShaderCode);

			std::filesystem::path fragmentShaderPath =
			    settings.shaderDirectories[i];
			fragmentShaderPath /= "frag.spv";
			auto fragShaderCode = readFile(fragmentShaderPath);
			graphicsPipelines[i].fragShaderModule =
			    createShaderModule(fragShaderCode);

			std::filesystem::path configFilePath =
			    settings.shaderDirectories[i];
			configFilePath /= "config";
			graphicsPipelines[i].specializationConstants =
			    readSpecializationConstants(configFilePath,
			                                graphicsPipelines[i].moduleName);
			graphicsPipelines[i].specializationConstants.data[0] =
			    static_cast<uint32_t>(settings.audioSize);
			graphicsPipelines[i].specializationConstants.data[1] =
			    settings.smoothingLevel;
		}
	}

	void createGraphicsPipelines() {
		VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
		vertexInputInfo.sType =
		    VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 0;
		vertexInputInfo.vertexAttributeDescriptionCount = 0;

		VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
		inputAssembly.sType =
		    VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		VkViewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)swapChainExtent.width;
		viewport.height = (float)swapChainExtent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissors = {};
		scissors.offset = {0, 0};
		scissors.extent = swapChainExtent;

		VkPipelineViewportStateCreateInfo viewportState = {};
		viewportState.sType =
		    VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissors;

		VkPipelineRasterizationStateCreateInfo rasterizer = {};
		rasterizer.sType =
		    VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;
		rasterizer.rasterizerDiscardEnable = VK_FALSE;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth = 1.0f;
		rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
		rasterizer.depthBiasEnable = VK_FALSE;

		VkPipelineMultisampleStateCreateInfo multisampling = {};
		multisampling.sType =
		    VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
		colorBlendAttachment.colorWriteMask =
		    VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
		    VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = true;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachment.dstColorBlendFactor =
		    VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

		VkPipelineColorBlendStateCreateInfo colorBlending = {};
		colorBlending.sType =
		    VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;

		VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
		pipelineLayoutInfo.sType =
		    VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 1;
		pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
		pipelineLayoutInfo.pushConstantRangeCount = 0;
		pipelineLayoutInfo.pPushConstantRanges = nullptr;

		if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr,
		                           &pipelineLayout) != VK_SUCCESS)
			throw std::runtime_error(__FILE__
			                         ": failed to create pipeline layout!");

		VkPipelineShaderStageCreateInfo
		    vertShaderStageInfos[graphicsPipelines.size()];
		VkPipelineShaderStageCreateInfo
		    fragShaderStageInfos[graphicsPipelines.size()];
		VkSpecializationInfo specializationInfos[graphicsPipelines.size()];
		VkPipelineShaderStageCreateInfo shaderStages[graphicsPipelines.size()]
		                                            [2];
		VkGraphicsPipelineCreateInfo pipelineInfos[graphicsPipelines.size()];
		VkPipeline pipelines[graphicsPipelines.size()];

		for (uint32_t i = 0; i < graphicsPipelines.size(); ++i) {
			pipelines[i] = graphicsPipelines[i].graphicsPipeline;

			specializationInfos[i] = {};
			specializationInfos[i].mapEntryCount =
			    graphicsPipelines[i]
			        .specializationConstants.specializationInfo.size();
			specializationInfos[i].pMapEntries =
			    graphicsPipelines[i]
			        .specializationConstants.specializationInfo.data();
			specializationInfos[i].dataSize =
			    graphicsPipelines[i].specializationConstants.data.size() *
			    sizeof(SpecializationConstant);
			specializationInfos[i].pData =
			    graphicsPipelines[i].specializationConstants.data.data();

			vertShaderStageInfos[i] = {};
			vertShaderStageInfos[i].sType =
			    VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			vertShaderStageInfos[i].stage = VK_SHADER_STAGE_VERTEX_BIT;
			vertShaderStageInfos[i].module =
			    graphicsPipelines[i].vertShaderModule;
			vertShaderStageInfos[i].pName = "main";
			vertShaderStageInfos[i].pSpecializationInfo =
			    &specializationInfos[i];

			fragShaderStageInfos[i] = {};
			fragShaderStageInfos[i].sType =
			    VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			fragShaderStageInfos[i].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
			fragShaderStageInfos[i].module =
			    graphicsPipelines[i].fragShaderModule;
			fragShaderStageInfos[i].pName =
			    graphicsPipelines[i].moduleName.c_str();
			fragShaderStageInfos[i].pSpecializationInfo =
			    &specializationInfos[i];

			graphicsPipelines[i].specializationConstants.data[2] =
			    swapChainExtent.width;
			graphicsPipelines[i].specializationConstants.data[3] =
			    swapChainExtent.height;

			shaderStages[i][0] = vertShaderStageInfos[i];
			shaderStages[i][1] = fragShaderStageInfos[i];

			pipelineInfos[i] = {};
			pipelineInfos[i].sType =
			    VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			pipelineInfos[i].stageCount = 2;
			pipelineInfos[i].pStages = shaderStages[i];
			pipelineInfos[i].pVertexInputState = &vertexInputInfo;
			pipelineInfos[i].pInputAssemblyState = &inputAssembly;
			pipelineInfos[i].pViewportState = &viewportState;
			pipelineInfos[i].pRasterizationState = &rasterizer;
			pipelineInfos[i].pMultisampleState = &multisampling;
			pipelineInfos[i].pDepthStencilState = nullptr;
			pipelineInfos[i].pColorBlendState = &colorBlending;
			pipelineInfos[i].pDynamicState = nullptr;
			pipelineInfos[i].layout = pipelineLayout;
			pipelineInfos[i].renderPass = renderPass;
			pipelineInfos[i].subpass = 0;
			pipelineInfos[i].basePipelineHandle = VK_NULL_HANDLE;
			pipelineInfos[i].basePipelineIndex = 0;
		}

		if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE,
		                              graphicsPipelines.size(), pipelineInfos,
		                              nullptr, pipelines))
			throw std::runtime_error(__FILE__
			                         ": failed to create graphics pipeline!");

		for (uint32_t i = 0; i < graphicsPipelines.size(); ++i)
			graphicsPipelines[i].graphicsPipeline = pipelines[i];
	}

	VkShaderModule createShaderModule(const std::vector<char>& shaderCode) {
		VkShaderModuleCreateInfo shaderModuleInfo = {};
		shaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		shaderModuleInfo.codeSize = shaderCode.size();
		shaderModuleInfo.pCode =
		    reinterpret_cast<const uint32_t*>(shaderCode.data());

		VkShaderModule shaderModule;
		if (vkCreateShaderModule(device, &shaderModuleInfo, nullptr,
		                         &shaderModule) != VK_SUCCESS)
			throw std::runtime_error(__FILE__
			                         ": failed to create shader module!");

		return shaderModule;
	}

	void createRenderPass() {
		VkAttachmentDescription colorAttachment = {};
		colorAttachment.format = swapChainImageFormat;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorAttachmentRef = {};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;

		VkSubpassDependency dependency = {};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
		                           VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &colorAttachment;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) !=
		    VK_SUCCESS)
			throw std::runtime_error(__FILE__
			                         ": failed to create render pass!");
	}

	void createFramebuffers() {
		swapChainFramebuffers.resize(swapChainImageViews.size());

		for (size_t i = 0; i < swapChainFramebuffers.size(); ++i) {
			VkImageView attachments[] = {swapChainImageViews[i]};

			VkFramebufferCreateInfo framebufferInfo = {};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = renderPass;
			framebufferInfo.attachmentCount = 1;
			framebufferInfo.pAttachments = attachments;
			framebufferInfo.width = swapChainExtent.width;
			framebufferInfo.height = swapChainExtent.height;
			framebufferInfo.layers = 1;

			if (vkCreateFramebuffer(device, &framebufferInfo, nullptr,
			                        &swapChainFramebuffers[i]) != VK_SUCCESS)
				throw std::runtime_error(__FILE__
				                         ": failed to create framebuffer!");
		}
	}

	void createCommandPool() {
		QueueFamilyIndices queueFamilyIndices =
		    findQueueFamilies(physicalDevice);

		VkCommandPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

		if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) !=
		    VK_SUCCESS)
			throw std::runtime_error(__FILE__
			                         ": failed to create command pool!");
	}

	void createCommandBuffers() {
		commandBuffers.resize(swapChainFramebuffers.size());

		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount =
		    static_cast<uint32_t>(commandBuffers.size());

		if (vkAllocateCommandBuffers(device, &allocInfo,
		                             commandBuffers.data()) != VK_SUCCESS)
			throw std::runtime_error(__FILE__
			                         ": failed to allocate command buffers!");

		for (size_t i = 0; i < commandBuffers.size(); ++i) {
			VkCommandBufferBeginInfo beginInfo = {};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
			beginInfo.pInheritanceInfo = nullptr;

			if (vkBeginCommandBuffer(commandBuffers[i], &beginInfo) !=
			    VK_SUCCESS)
				throw std::runtime_error(
				    __FILE__ ": failed to begin recording command buffer!");

			VkRenderPassBeginInfo renderPassInfo = {};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassInfo.renderPass = renderPass;
			renderPassInfo.framebuffer = swapChainFramebuffers[i];
			renderPassInfo.renderArea.offset = {0, 0};
			renderPassInfo.renderArea.extent = swapChainExtent;
			VkClearValue clearColor = {0.0f, 0.0f, 0.0f, 0.0f};
			renderPassInfo.clearValueCount = 1;
			renderPassInfo.pClearValues = &clearColor;

			vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo,
			                     VK_SUBPASS_CONTENTS_INLINE);

			for (const auto& graphicsPipeline : graphicsPipelines) {
				vkCmdBindPipeline(commandBuffers[i],
				                  VK_PIPELINE_BIND_POINT_GRAPHICS,
				                  graphicsPipeline.graphicsPipeline);

				vkCmdBindDescriptorSets(
				    commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS,
				    pipelineLayout, 0, 1, &descriptorSets[i], 0, nullptr);
				vkCmdDraw(commandBuffers[i], 6, 1, 0, 0);
			}

			vkCmdEndRenderPass(commandBuffers[i]);

			if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS)
				throw std::runtime_error(__FILE__
				                         ": failed to record command buffer!");
		}
	}

	void createSyncObjects() {
		imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

		VkSemaphoreCreateInfo semaphoreInfo = {};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo = {};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
			if (vkCreateSemaphore(device, &semaphoreInfo, nullptr,
			                      &imageAvailableSemaphores[i]) != VK_SUCCESS ||
			    vkCreateSemaphore(device, &semaphoreInfo, nullptr,
			                      &renderFinishedSemaphores[i]) != VK_SUCCESS ||
			    vkCreateFence(device, &fenceInfo, nullptr,
			                  &inFlightFences[i]) != VK_SUCCESS)
				throw std::runtime_error(__FILE__
				                         ": failed to create sync objects!");
		}
	}

	void cleanupSwapChain() {
		for (auto framebuffer : swapChainFramebuffers)
			vkDestroyFramebuffer(device, framebuffer, nullptr);

		vkFreeCommandBuffers(device, commandPool,
		                     static_cast<uint32_t>(commandBuffers.size()),
		                     commandBuffers.data());

		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		for (auto& graphicsPipeline : graphicsPipelines)
			vkDestroyPipeline(device, graphicsPipeline.graphicsPipeline,
			                  nullptr);

		vkDestroyRenderPass(device, renderPass, nullptr);

		for (auto imageView : swapChainImageViews)
			vkDestroyImageView(device, imageView, nullptr);

		vkDestroySwapchainKHR(device, swapChain, nullptr);
	}

	void recreateSwapChain() {
		while (glfwGetWindowAttrib(window, GLFW_ICONIFIED)) glfwWaitEvents();

		vkDeviceWaitIdle(device);

		cleanupSwapChain();

		createSwapchain();
		createImageViews();
		createRenderPass();
		createGraphicsPipelines();
		createFramebuffers();
		createCommandBuffers();
	}

	void createBackgroundImage() {
		createTextureImage(settings.backgroundImage, backgroundImage,
		                   backgroundImageMemory);
	}

	void createTextureImage(const std::filesystem::path& imagePath,
	                        VkImage& image, VkDeviceMemory& imageMemory) {
		size_t width, height, size;
		unsigned char** imgData;
		if (imagePath.empty()) {
			width = 1;
			height = 1;
			imgData = new unsigned char*[1];
			imgData[0] = new unsigned char[4]{0, 0, 0, 0};
		} else {
			imgData = readImg(imagePath, width, height);
		}
		size = width * height * 4;

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;

		createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
		                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		             stagingBuffer, stagingBufferMemory);

		void* data;
		vkMapMemory(device, stagingBufferMemory, 0, size, 0, &data);
		for (size_t y = 0; y < height; ++y)
			std::copy_n(imgData[y], width * 4,
			            reinterpret_cast<unsigned char*>(data) + y * width * 4);
		vkUnmapMemory(device, stagingBufferMemory);

		for (size_t y = 0; y < height; ++y) delete[] imgData[y];
		delete[] imgData;

		createImage(
		    width, height, VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM,
		    VK_IMAGE_TILING_OPTIMAL,
		    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, imageMemory);

		transitionImageLayout(image, VK_IMAGE_LAYOUT_UNDEFINED,
		                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		copyBufferToImage(stagingBuffer, image, static_cast<uint32_t>(width),
		                  static_cast<uint32_t>(height));
		transitionImageLayout(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		vkDestroyBuffer(device, stagingBuffer, nullptr);
		vkFreeMemory(device, stagingBufferMemory, nullptr);
	}

	uint32_t findMemoryType(uint32_t typeFilter,
	                        VkMemoryPropertyFlags properties) {
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

		for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
			if (typeFilter & (1 << i) &&
			    (memProperties.memoryTypes[i].propertyFlags & properties) ==
			        properties)
				return i;
		}

		throw std::runtime_error(__FILE__
		                         ": failed to find suitable memory type!");
	}

	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
	                  VkMemoryPropertyFlags properties, VkBuffer& buffer,
	                  VkDeviceMemory& bufferMemory) {
		VkBufferCreateInfo bufferInfo = {};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
			throw std::runtime_error(__FILE__ ": failed to create buffer!");

		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex =
		    findMemoryType(memRequirements.memoryTypeBits, properties);

		if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) !=
		    VK_SUCCESS)
			throw std::runtime_error(__FILE__
			                         ": failed to allocate buffer memory!");

		vkBindBufferMemory(device, buffer, bufferMemory, 0);
	}

	void createBufferView(VkDeviceSize size, VkFormat format, VkBuffer buffer,
	                      VkBufferView& bufferView) {
		VkBufferViewCreateInfo viewInfo = {};
		viewInfo.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
		viewInfo.buffer = buffer;
		viewInfo.format = format;
		viewInfo.offset = 0;
		viewInfo.range = size;

		if (vkCreateBufferView(device, &viewInfo, nullptr, &bufferView) !=
		    VK_SUCCESS)
			throw std::runtime_error(__FILE__
			                         ": failed to create buffer view!");
	}

	void createImage(uint32_t width, uint32_t height, VkImageType imageType,
	                 VkFormat format, VkImageTiling tiling,
	                 VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
	                 VkImage& image, VkDeviceMemory& imageMemory) {
		VkImageCreateInfo imageInfo = {};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = imageType;
		imageInfo.extent.width = width;
		imageInfo.extent.height = height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.format = format;
		imageInfo.tiling = tiling;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = usage;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS)
			throw std::runtime_error(__FILE__ ": failed to create image!");

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(device, image, &memRequirements);

		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex =
		    findMemoryType(memRequirements.memoryTypeBits, properties);

		if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) !=
		    VK_SUCCESS)
			throw std::runtime_error(__FILE__
			                         ": failed to allocate image memory!");

		vkBindImageMemory(device, image, imageMemory, 0);
	}

	VkCommandBuffer beginSingleTimeCommands() {
		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = commandPool;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(commandBuffer, &beginInfo);

		return commandBuffer;
	}

	void endSingleTimeCommands(VkCommandBuffer commandBuffer) {
		vkEndCommandBuffer(commandBuffer);

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(graphicsQueue);

		vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
	}

	void transitionImageLayout(VkImage image, VkImageLayout oldLayout,
	                           VkImageLayout newLayout) {
		VkCommandBuffer commandBuffer = beginSingleTimeCommands();

		VkImageMemoryBarrier barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;

		VkPipelineStageFlags srcStage;
		VkPipelineStageFlags dstStage;

		if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
		    newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		} else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
		           newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		} else {
			throw std::invalid_argument(__FILE__
			                            ": unsupported layout transition!");
		}

		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = 0;

		vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0, 0, nullptr,
		                     0, nullptr, 1, &barrier);

		endSingleTimeCommands(commandBuffer);
	}

	void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width,
	                       uint32_t height) {
		VkCommandBuffer commandBuffer = beginSingleTimeCommands();

		VkBufferImageCopy region = {};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;

		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;

		region.imageOffset = {0, 0, 0};
		region.imageExtent = {width, height, 1};

		vkCmdCopyBufferToImage(commandBuffer, buffer, image,
		                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
		                       &region);

		endSingleTimeCommands(commandBuffer);
	}

	void createBackgroundImageView() {
		backgroundImageView =
		    createImageView(backgroundImage, VK_FORMAT_R8G8B8A8_UNORM);
	}

	VkImageView createImageView(VkImage image, VkFormat format) {
		VkImageViewCreateInfo viewInfo = {};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = format;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		VkImageView imageView;
		if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) !=
		    VK_SUCCESS)
			throw std::runtime_error(__FILE__ ": failed to create image view!");

		return imageView;
	}

	void createBackgroundImageSampler() {
		VkSamplerCreateInfo samplerInfo = {};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.maxAnisotropy = 1;
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = 0.0f;

		if (vkCreateSampler(device, &samplerInfo, nullptr,
		                    &backgroundImageSampler) != VK_SUCCESS)
			throw std::runtime_error(__FILE__
			                         ": failed to create image sampler!");
	}

	void createDescriptorSetLayout() {
		VkDescriptorSetLayoutBinding volumeLayoutBinding = {};
		volumeLayoutBinding.binding = 0;
		volumeLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		volumeLayoutBinding.descriptorCount = 1;
		volumeLayoutBinding.pImmutableSamplers = nullptr;
		volumeLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutBinding lAudioBufferLayoutBinding = {};
		lAudioBufferLayoutBinding.binding = 1;
		lAudioBufferLayoutBinding.descriptorType =
		    VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
		lAudioBufferLayoutBinding.descriptorCount = 1;
		lAudioBufferLayoutBinding.pImmutableSamplers = nullptr;
		lAudioBufferLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutBinding rAudioBufferLayoutBinding = {};
		rAudioBufferLayoutBinding.binding = 2;
		rAudioBufferLayoutBinding.descriptorType =
		    VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
		rAudioBufferLayoutBinding.descriptorCount = 1;
		rAudioBufferLayoutBinding.pImmutableSamplers = nullptr;
		rAudioBufferLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutBinding backgroundSamplerLayoutBinding = {};
		backgroundSamplerLayoutBinding.binding = 3;
		backgroundSamplerLayoutBinding.descriptorCount = 1;
		backgroundSamplerLayoutBinding.descriptorType =
		    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		backgroundSamplerLayoutBinding.pImmutableSamplers = nullptr;
		backgroundSamplerLayoutBinding.stageFlags =
		    VK_SHADER_STAGE_FRAGMENT_BIT;

		std::array<VkDescriptorSetLayoutBinding, 4> bindings = {
		    volumeLayoutBinding, lAudioBufferLayoutBinding,
		    rAudioBufferLayoutBinding, backgroundSamplerLayoutBinding};

		VkDescriptorSetLayoutCreateInfo layoutInfo = {};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
		layoutInfo.pBindings = bindings.data();

		if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr,
		                                &descriptorSetLayout) != VK_SUCCESS)
			throw std::runtime_error(
			    __FILE__ ": failed to create descriptor set layout!");
	}

	void createAudioBuffers() {
		VkDeviceSize bufferSize = settings.audioSize * sizeof(float);

		volumeBuffers.resize(swapChainImages.size());
		volumeBufferMemory.resize(swapChainImages.size());

		lAudioBuffers.resize(swapChainImages.size());
		lAudioBufferMemory.resize(swapChainImages.size());
		lAudioBufferViews.resize(swapChainImages.size());

		rAudioBuffers.resize(swapChainImages.size());
		rAudioBufferMemory.resize(swapChainImages.size());
		rAudioBufferViews.resize(swapChainImages.size());

		for (size_t i = 0; i < volumeBuffers.size(); ++i) {
			createBuffer(2 * sizeof(float), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			             volumeBuffers[i], volumeBufferMemory[i]);

			createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT,
			             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			             lAudioBuffers[i], lAudioBufferMemory[i]);

			createBufferView(bufferSize, VK_FORMAT_R32_SFLOAT, lAudioBuffers[i],
			                 lAudioBufferViews[i]);

			createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT,
			             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			             rAudioBuffers[i], rAudioBufferMemory[i]);

			createBufferView(bufferSize, VK_FORMAT_R32_SFLOAT, rAudioBuffers[i],
			                 rAudioBufferViews[i]);
		}
	}

	void updateAudioBuffers(const AudioData& audioData, uint32_t currentFrame) {
		void* data;

		vkMapMemory(device, volumeBufferMemory[currentFrame], 0,
		            2 * sizeof(float), 0, &data);
		reinterpret_cast<float*>(data)[0] = audioData.lVolume;
		reinterpret_cast<float*>(data)[1] = audioData.rVolume;
		vkUnmapMemory(device, volumeBufferMemory[currentFrame]);

		vkMapMemory(device, lAudioBufferMemory[currentFrame], 0,
		            settings.audioSize * sizeof(float), 0, &data);
		std::copy_n(audioData.lBuffer, settings.audioSize,
		            reinterpret_cast<float*>(data));
		vkUnmapMemory(device, lAudioBufferMemory[currentFrame]);

		vkMapMemory(device, rAudioBufferMemory[currentFrame], 0,
		            settings.audioSize * sizeof(float), 0, &data);
		std::copy_n(audioData.rBuffer, settings.audioSize,
		            reinterpret_cast<float*>(data));
		vkUnmapMemory(device, rAudioBufferMemory[currentFrame]);
	}

	void createDescriptorPool() {
		std::array<VkDescriptorPoolSize, 4> poolSizes = {};
		poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSizes[0].descriptorCount =
		    static_cast<uint32_t>(swapChainImages.size());
		poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
		poolSizes[1].descriptorCount =
		    static_cast<uint32_t>(swapChainImages.size());
		poolSizes[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
		poolSizes[2].descriptorCount =
		    static_cast<uint32_t>(swapChainImages.size());
		poolSizes[3].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSizes[3].descriptorCount =
		    static_cast<uint32_t>(swapChainImages.size());

		VkDescriptorPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = static_cast<uint32_t>(swapChainImages.size());

		if (vkCreateDescriptorPool(device, &poolInfo, nullptr,
		                           &descriptorPool) != VK_SUCCESS)
			throw std::runtime_error(__FILE__
			                         ": failed to create descriptor pool!");
	}

	void createDescriptorSets() {
		std::vector<VkDescriptorSetLayout> layouts(swapChainImages.size(),
		                                           descriptorSetLayout);
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorPool;
		allocInfo.descriptorSetCount =
		    static_cast<uint32_t>(swapChainImages.size());
		allocInfo.pSetLayouts = layouts.data();

		descriptorSets.resize(swapChainImages.size());
		if (vkAllocateDescriptorSets(device, &allocInfo,
		                             descriptorSets.data()) != VK_SUCCESS)
			throw std::runtime_error(__FILE__
			                         ": failed to allocate descriptor sets!");

		for (size_t i = 0; i < swapChainImages.size(); ++i) {
			VkDescriptorBufferInfo volumeBufferInfo = {};
			volumeBufferInfo.buffer = volumeBuffers[i];
			volumeBufferInfo.offset = 0;
			volumeBufferInfo.range = 2 * sizeof(float);

			VkDescriptorImageInfo imageInfo = {};
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfo.imageView = backgroundImageView;
			imageInfo.sampler = backgroundImageSampler;

			std::array<VkWriteDescriptorSet, 4> descriptorWrites = {};
			descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[0].dstSet = descriptorSets[i];
			descriptorWrites[0].dstBinding = 0;
			descriptorWrites[0].dstArrayElement = 0;
			descriptorWrites[0].descriptorType =
			    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			descriptorWrites[0].descriptorCount = 1;
			descriptorWrites[0].pBufferInfo = &volumeBufferInfo;

			descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[1].dstSet = descriptorSets[i];
			descriptorWrites[1].dstBinding = 1;
			descriptorWrites[1].dstArrayElement = 0;
			descriptorWrites[1].descriptorType =
			    VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
			descriptorWrites[1].descriptorCount = 1;
			descriptorWrites[1].pTexelBufferView = &lAudioBufferViews[i];

			descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[2].dstSet = descriptorSets[i];
			descriptorWrites[2].dstBinding = 2;
			descriptorWrites[2].dstArrayElement = 0;
			descriptorWrites[2].descriptorType =
			    VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
			descriptorWrites[2].descriptorCount = 1;
			descriptorWrites[2].pTexelBufferView = &rAudioBufferViews[i];

			descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[3].dstSet = descriptorSets[i];
			descriptorWrites[3].dstBinding = 3;
			descriptorWrites[3].dstArrayElement = 0;
			descriptorWrites[3].descriptorType =
			    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrites[3].descriptorCount = 1;
			descriptorWrites[3].pImageInfo = &imageInfo;

			vkUpdateDescriptorSets(
			    device, static_cast<uint32_t>(descriptorWrites.size()),
			    descriptorWrites.data(), 0, nullptr);
		}
	}

	// Static member functions

	static VKAPI_ATTR VkBool32 VKAPI_CALL
	debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	              VkDebugUtilsMessageTypeFlagsEXT messageType,
	              const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	              void* userData) {
		std::cerr << "validation layer: " << pCallbackData->pMessage
		          << std::endl;
		return VK_FALSE;
	}

	static std::vector<char> readFile(const std::filesystem::path& filePath) {
		std::ifstream file(filePath, std::ios::binary | std::ios::ate);

		if (!file.is_open())
			throw std::runtime_error(__FILE__ ": failed to open file!");

		size_t fileSize = static_cast<size_t>(file.tellg());
		std::vector<char> buffer(fileSize);

		file.seekg(0);
		file.read(buffer.data(), fileSize);

		file.close();

		return buffer;
	}

	static SpecializationConstants readSpecializationConstants(
	    const std::filesystem::path& configFilePath, std::string& moduleName) {
		SpecializationConstants specializationConstants;

		specializationConstants.data.resize(4);

		for (uint32_t offset = 0; offset < 4; ++offset) {
			VkSpecializationMapEntry mapEntry = {};
			mapEntry.constantID = offset;
			mapEntry.offset = offset * sizeof(SpecializationConstant);
			mapEntry.size = sizeof(SpecializationConstant);

			specializationConstants.specializationInfo.push_back(mapEntry);
		}

		std::ifstream file(configFilePath);
		if (!file.is_open()) {
			std::cerr << "shader configuration file not found!" << std::endl;
			return specializationConstants;
		}

		std::string line;

		while (std::getline(file, line).good()) {
			size_t position;

			// Remove comments
			if ((position = line.find("//")) != std::string::npos)
				line.resize(position);

			// Remove whitespaces
			line.resize(std::remove_if(line.begin(), line.end(), isspace) -
			            line.begin());

			if (line.substr(0, 6) == "module") {
				moduleName = line.substr(8, line.size() - 8 - 1);
				continue;
			}

			position = line.find(')');

			if (position == std::string::npos) continue;

			uint32_t id = std::stoi(line.substr(4, position - 4));

			size_t equalSignPos = line.find('=', position);

			SpecializationConstant value;
			if (line.substr(position + 1, 3) == "int")
				value = std::stoi(line.substr(equalSignPos + 1));
			else if (line.substr(position + 1, 5) == "float")
				value = std::stof(line.substr(equalSignPos + 1));
			else
				throw std::invalid_argument(
				    __FILE__
				    ": invalid variable type in shader configuration file!");

			VkSpecializationMapEntry mapEntry = {};
			mapEntry.constantID = id;
			mapEntry.offset = specializationConstants.data.size() *
			                  sizeof(SpecializationConstant);
			mapEntry.size = sizeof(SpecializationConstant);

			specializationConstants.data.push_back(value);
			specializationConstants.specializationInfo.push_back(mapEntry);
		}

		file.close();

		specializationConstants.data.shrink_to_fit();
		specializationConstants.specializationInfo.shrink_to_fit();

		return specializationConstants;
	}

	static void framebufferResizeCallback(GLFWwindow* window, int width,
	                                      int height) {
		auto renderer =
		    reinterpret_cast<RendererImpl*>(glfwGetWindowUserPointer(window));
		renderer->framebufferResized = true;
	}
};

void Renderer::init(const RenderSettings& renderSettings) {
	rendererImpl = new RendererImpl(renderSettings);
}

bool Renderer::drawFrame(const AudioData& audioData) {
	return rendererImpl->drawFrame(audioData);
}

void Renderer::cleanup() { delete rendererImpl; }

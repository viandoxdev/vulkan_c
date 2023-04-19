#define GLFW_INCLUDE_VULKAN
// List of vector implementions: (qualifier is used in functions name, but shouldn't
// be used in user code)
//  (contained type, vector type, qualifier)
#define VECTOR_IMPL_LIST \
    (const char *, ConstStringVec, const_string), (VkImage, VkImageVec, vk_image), (VkImageView, VkImageViewVec, vk_image_view), \
        (VkFramebuffer, VkFramebufferVec, vk_framebuffer), (VkLayerProperties, VkLayerPropertiesVec, vk_layer_properties), \
        (VkPhysicalDevice, VkPhysicalDeviceVec, vk_physical_device), \
        (VkExtensionProperties, VkExtensionPropertiesVec, vk_extension_properties)
#include "assert.h"
#include "log.h"
#include "macro_utils.h"
#include "proxies.h"
#include "vk_enum_string_helper.h"

#include <GLFW/glfw3.h>
#include <limits.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>

// vector.h must be included last (or actually after vulkan.h), since it references some
// included types in the implemention of the vectors it generates
// clang-format off
#include "vector.h"
// clang-format on

// run expr, and assert that the returned VkResult is VK_SUCCESS
#define vk_try(expr, fmt, ...) \
    do { \
        VkResult _res = expr; \
        assert_eq(_res, VK_SUCCESS, fmt " (%s)", __VA_ARGS__ __VA_OPT__(, ) string_VkResult(_res)); \
    } while (false)
#define vk_get_vec(vec, expr) \
    do { \
        uint32_t _count; \
        uint32_t *count = &_count; \
        void *ptr = NULL; \
        expr; \
        *(vec) = (typeof(*(vec)))vec_init(); \
        vec_grow(vec, _count); \
        ptr = (vec)->data; \
        expr; \
        (vec)->len = _count; \
    } while (false)

// Structs
typedef struct {
    GLFWwindow *win;
} Window;

typedef struct {
    int64_t graphics;
    int64_t present;
} QueueFamilyIndices;

#define MAX_QUEUE_COUNT 2

typedef struct {
    VkSurfaceCapabilitiesKHR capabilities;
    uint32_t formats_count;
    VkSurfaceFormatKHR *formats;
    uint32_t present_modes_count;
    VkPresentModeKHR *present_modes;
} SwapChainSupportDetails;

typedef struct {
    VkSurfaceFormatKHR format;
    VkPresentModeKHR present_mode;
    VkExtent2D extent;
    uint32_t image_count;
    VkBool32 composite_alpha;
    VkSurfaceTransformFlagBitsKHR transform;
} SwapChainConfig;

typedef struct {
    VkInstance instance;
    // Debug messenger used to route vulkan messages through the logger
    VkDebugUtilsMessengerEXT debug_messenger;
    // Window surface
    VkSurfaceKHR surface;
    VkPhysicalDevice physical_device;
    QueueFamilyIndices queue_family_indices;
    SwapChainSupportDetails swapchain_support;

    VkDevice device;
    SwapChainConfig config;
    VkSwapchainKHR swapchain;
    VkImageVec images;
    VkImageViewVec image_views;
    VkFramebufferVec framebuffers;
    VkRenderPass render_pass;
    VkPipelineLayout pipeline_layout;
    VkPipeline graphics_pipeline;
    VkCommandPool command_pool;
    VkCommandBuffer command_buffer;
    VkSemaphore image_available_semaphore;
    VkSemaphore render_finished_semaphore;
    VkFence in_flight_fence;

    VkQueue graphics_queue;
    VkQueue present_queue;
} GraphicContext;

// Vulkan configuration constants

static const char *VALIDATION_LAYERS[] = {"VK_LAYER_KHRONOS_validation"};
static const uint32_t VALIDATION_LAYER_COUNT = sizeof(VALIDATION_LAYERS) / sizeof(char *);

static const char *REQUIRED_EXTENSIONS[] = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
static const uint32_t REQUIRED_EXTENSIONS_COUNT = sizeof(REQUIRED_EXTENSIONS) / sizeof(char *);

static const char *REQUIRED_DEVICE_EXTENSIONS[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
static const uint32_t REQUIRED_DEVICE_EXTENSIONS_COUNT = sizeof(REQUIRED_DEVICE_EXTENSIONS) / sizeof(char *);

__attribute__((aligned(4))) static const uint8_t VERTEX_SHADER[] = {
#include "include/shader.vert.spv.bytes"
};
static const size_t VERTEX_SHADER_LEN = sizeof(VERTEX_SHADER) / sizeof(uint8_t);
__attribute__((aligned(4))) static const uint8_t FRAGMENT_SHADER[] = {
#include "include/shader.frag.spv.bytes"
};
static const size_t FRAGMENT_SHADER_LEN = sizeof(FRAGMENT_SHADER) / sizeof(uint8_t);

SwapChainSupportDetails swapchain_support_details_init(VkPhysicalDevice dev, VkSurfaceKHR surface) {
    SwapChainSupportDetails details = {0};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev, surface, &details.capabilities);
    vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &details.formats_count, NULL);
    vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &details.present_modes_count, NULL);
    if (details.formats_count != 0) {
        details.formats = malloc(details.formats_count * sizeof(VkSurfaceFormatKHR));
        assert_alloc(details.formats);
        vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &details.formats_count, details.formats);
    }
    if (details.present_modes_count != 0) {
        details.present_modes = malloc(details.present_modes_count * sizeof(VkPresentModeKHR));
        assert_alloc(details.present_modes);
        vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &details.present_modes_count, details.present_modes);
    }
    return details;
}

void swapchain_support_details_drop(SwapChainSupportDetails swpd) {
    if (swpd.formats_count != 0)
        free(swpd.formats);
    if (swpd.present_modes_count != 0)
        free(swpd.present_modes);
}

QueueFamilyIndices queue_family_indices_init(VkPhysicalDevice dev, VkSurfaceKHR surface) {
    QueueFamilyIndices idx;
    idx.graphics = -1;
    idx.present = -1;

    uint32_t count;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, NULL);
    VkQueueFamilyProperties *props = malloc(count * sizeof(VkQueueFamilyProperties));
    assert_alloc(props);

    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, props);
    for (uint32_t i = 0; i < count; i++) {
        VkQueueFamilyProperties queue = props[i];
        VkBool32 present_support;
        vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &present_support);

        if (queue.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            idx.graphics = i;
        }
        if (present_support) {
            idx.present = i;
        }
    }

    free(props);
    return idx;
}

bool queue_family_indices_complete(QueueFamilyIndices *idx) { return idx->present >= 0 && idx->graphics >= 0; }

VkResult create_shader_module(VkDevice dev, const uint32_t *data, size_t len, VkShaderModule *module) {
    VkShaderModuleCreateInfo create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = len;
    create_info.pCode = data;
    return vkCreateShaderModule(dev, &create_info, NULL, module);
}

// Debug callback
static VKAPI_ATTR VkBool32 VKAPI_CALL validation_layers_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *pUserData
) {
#ifndef LOG_DISABLE
    LogSeverity sev;
    switch (messageSeverity) {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        sev = Trace;
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        sev = Debug;
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        sev = Warning;
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        sev = Error;
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_FLAG_BITS_MAX_ENUM_EXT:
        sev = Info;
        break;
    }

    const char *type;
    switch (messageType) {
    case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
        type = "general";
        break;
    case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
        type = "validation";
        break;
    case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
        type = "performance";
        break;
    default:
        type = "???";
        break;
    }

    _log_severity(sev, type, "vulkan", -1, "%s", pCallbackData->pMessage);
#endif // LOG_DISABLE
    return VK_FALSE;
}

static inline VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info() {
    VkDebugUtilsMessengerCreateInfoEXT create_info = {0};

    create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT;
    create_info.pfnUserCallback = validation_layers_debug_callback;
    create_info.pUserData = NULL;

    return create_info;
}

// Configure swapchain according to what it supports and the window
static inline SwapChainConfig configure_swapchain(SwapChainSupportDetails *details, Window *win) {
    SwapChainConfig cfg;

    // Format
    // Default to the first one
    cfg.format = details->formats[0];
    // Prefer BGRA8 SRGB if available
    for (uint32_t i = 0; i < details->formats_count; i++) {
        VkSurfaceFormatKHR format = details->formats[i];
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR) {
            cfg.format = format;
        }
    }

    // Present mode
    // Default to FIFO (always available)
    cfg.present_mode = VK_PRESENT_MODE_FIFO_KHR;
    // Prefer mailbox if possible
    for (uint32_t i = 0; i < details->present_modes_count; i++) {
        VkPresentModeKHR mode = details->present_modes[i];
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            cfg.present_mode = mode;
        }
    }

    // Extent
    if (details->capabilities.currentExtent.width != UINT32_MAX) {
        cfg.extent = details->capabilities.currentExtent;
    } else {
        int w, h;
        glfwGetFramebufferSize(win->win, &w, &h);

        VkSurfaceCapabilitiesKHR *cap = &details->capabilities;

        cfg.extent.width = w < cap->minImageExtent.width   ? cap->minImageExtent.width
                           : w > cap->maxImageExtent.width ? cap->maxImageExtent.width
                                                           : w;
        cfg.extent.height = h < cap->minImageExtent.height   ? cap->minImageExtent.height
                            : h > cap->maxImageExtent.height ? cap->maxImageExtent.height
                                                             : w;
    }

    // Image count
    cfg.image_count = details->capabilities.minImageCount + 1;
    if (details->capabilities.maxImageCount > 0 && cfg.image_count > details->capabilities.maxImageCount) {
        cfg.image_count = details->capabilities.maxImageCount;
    }

    // Alpha
    cfg.composite_alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if (details->capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR) {
        cfg.composite_alpha = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
    }

    cfg.transform = details->capabilities.currentTransform;

    return cfg;
}

static inline VkResult create_swapchain(
    VkDevice dev,
    SwapChainConfig *cfg,
    VkSurfaceKHR surface,
    QueueFamilyIndices *idx,
    VkSwapchainKHR previous,
    VkSwapchainKHR *new
) {
    VkSwapchainCreateInfoKHR create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = surface;
    create_info.minImageCount = cfg->image_count;
    create_info.imageFormat = cfg->format.format;
    create_info.imageColorSpace = cfg->format.colorSpace;
    create_info.imageExtent = cfg->extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    create_info.preTransform = cfg->transform;
    // TODO: check that
    create_info.compositeAlpha = cfg->composite_alpha;
    create_info.presentMode = cfg->present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = previous;

    uint32_t queue_family_indices[2] = {idx->graphics, idx->present};
    if (idx->graphics != idx->present) {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    } else {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    return vkCreateSwapchainKHR(dev, &create_info, NULL, new);
}

GraphicContext ctx_init(const char *app_name, Window *win) {
    GraphicContext res = {0};

    ConstStringVec required_exts = vec_init();
    ConstStringVec enabled_layers = vec_init();

    // Required extensions
    {
        uint32_t glfw_ext_count;
        const char **glfw_exts = glfwGetRequiredInstanceExtensions(&glfw_ext_count);

        vec_grow(&required_exts, glfw_ext_count + REQUIRED_EXTENSIONS_COUNT);

        vec_push_array(&required_exts, glfw_exts, glfw_ext_count);
        vec_push_array(&required_exts, REQUIRED_EXTENSIONS, REQUIRED_EXTENSIONS_COUNT);
    }

    // Validation layers
#ifdef ENABLE_VALIDATION_LAYERS
    {
        VkLayerPropertiesVec supported_layers;
        vk_get_vec(&supported_layers, vkEnumerateInstanceLayerProperties(count, ptr));

        vec_grow(&enabled_layers, supported_layers.len);

        for (uint32_t i = 0; i < VALIDATION_LAYER_COUNT; i++) {
            const char *layer_name = VALIDATION_LAYERS[i];
            bool found = false;
            for (uint32_t j = 0; j < supported_layers.len; j++) {
                if (strcmp(layer_name, supported_layers.data[j].layerName) == 0) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                log_warn("Validation layer '%s' requested, but not available", layer_name);
            } else {
                vec_push(&enabled_layers, (char *)layer_name);
            }
        }

        vec_drop(supported_layers);
    }
#endif // ENABLE_VALIDATION_LAYERS

    // Instance
    {
        VkApplicationInfo appinfo = {0};
        appinfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appinfo.pApplicationName = app_name;
        appinfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appinfo.pEngineName = "None";
        appinfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appinfo.apiVersion = VK_API_VERSION_1_0;

        // Log the supported ext ensions
#ifndef LOG_DISABLE
        {
            VkExtensionPropertiesVec supported_exts;
            vk_get_vec(&supported_exts, vkEnumerateInstanceExtensionProperties(NULL, count, ptr));

            log_info("Availables vulkan extensions:");
            for (uint32_t i = 0; i < supported_exts.len; i++) {
                log_info("    %s", supported_exts.data[i].extensionName);
            }

            vec_drop(supported_exts);
        }
#endif // LOG_DISABLE

        VkDebugUtilsMessengerCreateInfoEXT debug_create_info = debug_messenger_create_info();

        VkInstanceCreateInfo create_info = {0};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.pApplicationInfo = &appinfo;
        create_info.enabledExtensionCount = required_exts.len;
        create_info.ppEnabledExtensionNames = required_exts.data;
        create_info.pNext = &debug_create_info;

#ifdef ENABLE_VALIDATION_LAYERS
        create_info.enabledLayerCount = enabled_layers.len;
        create_info.ppEnabledLayerNames = enabled_layers.data;
        log_info("Enabled validation layers:");
        for (uint32_t i = 0; i < enabled_layers.len; i++) {
            log_info("    %s", enabled_layers.data[i]);
        }
#else
        create_info.enabledLayerCount = 0;
#endif

        vk_try(vkCreateInstance(&create_info, NULL, &res.instance), "Failed to create vulkan instance");

        log_info("Enabled vulkan extensions:");
        for (uint32_t i = 0; i < required_exts.len; i++) {
            log_info("    %s", required_exts.data[i]);
        }
    }

    // Debug messenger
    {
        VkDebugUtilsMessengerCreateInfoEXT create_info = debug_messenger_create_info();
        vk_try(
            CreateDebugUtilsMessengerEXT(res.instance, &create_info, NULL, &res.debug_messenger),
            "Failed to create debug messenger"
        );
    }

    // Surface
    vk_try(glfwCreateWindowSurface(res.instance, win->win, NULL, &res.surface), "Failed to create window surface");

    // Physical Device
    {
        res.physical_device = VK_NULL_HANDLE;

        VkPhysicalDeviceVec devices;
        vk_get_vec(&devices, vkEnumeratePhysicalDevices(res.instance, count, ptr));

        if (devices.len == 0) {
            log_error("No vulkan device found.");
            exit(1);
        }

        char preferred_device_name[256] = {0};
        {
            FILE *override_file = fopen("preferred_device", "r");
            if (override_file != NULL) {
                uint32_t len = fread(preferred_device_name, 1, 255, override_file);

                // Strip ending newline if present
                if (preferred_device_name[len - 1] == '\n') {
                    preferred_device_name[len - 1] = '\0';
                }

                log_info("Preferred device: '%s'", preferred_device_name);
                fclose(override_file);
            }
        }

        int32_t max_score = -1;
        VkPhysicalDeviceProperties final_device_props;
        log_info("Suitable vulkan devices:");
        for (uint32_t i = 0; i < devices.len; i++) {
            VkPhysicalDevice dev = devices.data[i];
            VkPhysicalDeviceProperties props;
            VkPhysicalDeviceFeatures feats;
            QueueFamilyIndices idx;
            VkExtensionPropertiesVec device_extensions;

            idx = queue_family_indices_init(dev, res.surface);
            vkGetPhysicalDeviceProperties(dev, &props);
            vkGetPhysicalDeviceFeatures(dev, &feats);
            vk_get_vec(&device_extensions, vkEnumerateDeviceExtensionProperties(dev, NULL, count, ptr));

            // Conditions preventing this device from being choosen altogether
            {
                // Make sure the device supports all the required queue families
                if (!queue_family_indices_complete(&idx)) {
                    log_info("    (skipping '%s': missing queue family)", props.deviceName);
                    goto skip_device;
                }

                // Make sure the device supports all the required device extensions
                for (int i = 0; i < REQUIRED_DEVICE_EXTENSIONS_COUNT; i++) {
                    const char *ext = REQUIRED_DEVICE_EXTENSIONS[i];
                    bool found = false;
                    for (int j = 0; j < device_extensions.len; j++) {
                        if (strcmp(ext, device_extensions.data[j].extensionName) == 0) {
                            found = true;
                            break;
                        }
                    }

                    // Required extensions not supported, skip
                    if (!found) {
                        log_info("    (skipping '%s': extension %s not supported)", props.deviceName, ext);
                        goto skip_device;
                    }
                }

                bool swapchain_adequate;
                {
                    SwapChainSupportDetails details = swapchain_support_details_init(dev, res.surface);
                    swapchain_adequate = details.formats_count != 0 && details.present_modes_count != 0;
                    swapchain_support_details_drop(details);
                }

                if (!swapchain_adequate) {
                    log_info("    (skipping '%s': swapchain isn't adequate)", props.deviceName);
                    goto skip_device;
                }
            }

            // The device is valid, now compute its score to see if it is the best one.
            int32_t score = 0;
            {
                // Compute score
                switch (props.deviceType) {
                case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                    score += 10;
                    break;
                case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                    score += 5;
                    break;
                default:
                    break;
                }

                if (strstr(props.deviceName, preferred_device_name)) {
                    score = 9999;
                }
            }

            if (score >= 9999) {
                log_info("    '%s' (score: %d, preferred)", props.deviceName, score);
            } else {
                log_info("    '%s' (score: %d)", props.deviceName, score);
            }

            if (score > max_score) {
                max_score = score;
                res.physical_device = dev;
                res.queue_family_indices = idx;
                final_device_props = props;
            }

        skip_device:
            vec_drop(device_extensions);
        }

        vec_drop(devices);

        if (res.physical_device == VK_NULL_HANDLE) {
            log_error("Couldn't find suitable vulkan device.");
            exit(1);
        } else {
            res.swapchain_support = swapchain_support_details_init(res.physical_device, res.surface);
            log_info("Selected vulkan device: '%s'", final_device_props.deviceName);
        }
    }

    // Device
    {
        float priority = 1.0;
        uint32_t queue_indices[MAX_QUEUE_COUNT] = {
            res.queue_family_indices.graphics,
            res.queue_family_indices.present,
        };
        VkDeviceQueueCreateInfo queue_create_infos[MAX_QUEUE_COUNT] = {0};
        uint32_t queue_count = 0;

        for (int i = 0; i < MAX_QUEUE_COUNT; i++) {
            uint32_t index = queue_indices[i];
            bool found = false;
            for (int j = 0; i < queue_count; j++) {
                if (queue_create_infos[j].queueFamilyIndex == index) {
                    found = true;
                    break;
                }
            }
            if (found) {
                continue;
            }

            queue_create_infos[queue_count].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_create_infos[queue_count].queueFamilyIndex = index;
            queue_create_infos[queue_count].queueCount = 1;
            queue_create_infos[queue_count].pQueuePriorities = &priority;
            queue_count++;
        }

        VkPhysicalDeviceFeatures feats = {0};

        VkDeviceCreateInfo create_info = {0};
        create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        create_info.pQueueCreateInfos = queue_create_infos;
        create_info.queueCreateInfoCount = queue_count;
        create_info.pEnabledFeatures = &feats;
        create_info.enabledExtensionCount = REQUIRED_DEVICE_EXTENSIONS_COUNT;
        create_info.ppEnabledExtensionNames = REQUIRED_DEVICE_EXTENSIONS;
#ifdef ENABLE_VALIDATION_LAYERS
        create_info.enabledLayerCount = enabled_layers.len;
        create_info.ppEnabledLayerNames = enabled_layers.data;
#else
        create_info.enabledLayerCount = 0;
#endif

        vk_try(vkCreateDevice(res.physical_device, &create_info, NULL, &res.device), "Failed to create logical device");

        vkGetDeviceQueue(res.device, res.queue_family_indices.graphics, 0, &res.graphics_queue);
        vkGetDeviceQueue(res.device, res.queue_family_indices.present, 0, &res.present_queue);
    }

    // Swapchain
    {
        res.config = configure_swapchain(&res.swapchain_support, win);
        vk_try(
            create_swapchain(res.device, &res.config, res.surface, &res.queue_family_indices, VK_NULL_HANDLE, &res.swapchain),
            "Failed to create swapchain"
        );

        vk_get_vec(&res.images, vkGetSwapchainImagesKHR(res.device, res.swapchain, count, ptr));
    }

    // Image views
    {
        res.image_views = (VkImageViewVec)vec_init();
        vec_grow(&res.image_views, res.images.len);
        for (size_t i = 0; i < res.images.len; i++) {
            VkImageViewCreateInfo create_info = {0};
            create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            create_info.image = res.images.data[i];
            create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            create_info.format = res.config.format.format;
            create_info.components = (VkComponentMapping){
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            };
            create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            create_info.subresourceRange.baseMipLevel = 0;
            create_info.subresourceRange.levelCount = 1;
            create_info.subresourceRange.baseArrayLayer = 0;
            create_info.subresourceRange.layerCount = 1;

            vk_try(
                vkCreateImageView(res.device, &create_info, NULL, &res.image_views.data[i]),
                "Failed to create image view #%u",
                i
            );
        }
        res.image_views.len = res.images.len;
    }

    // Render Pass
    {
        VkAttachmentDescription color_attachment = {0};
        color_attachment.format = res.config.format.format;
        color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference color_attachment_reference = {0};
        color_attachment_reference.attachment = 0;
        color_attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {0};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment_reference;

        VkSubpassDependency dependency = {0};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo create_info = {0};
        create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        create_info.attachmentCount = 1;
        create_info.pAttachments = &color_attachment;
        create_info.subpassCount = 1;
        create_info.pSubpasses = &subpass;
        create_info.dependencyCount = 1;
        create_info.pDependencies = &dependency;

        vk_try(vkCreateRenderPass(res.device, &create_info, NULL, &res.render_pass), "Failed to create render pass");
    }

    // Graphic pipeline
    {
        VkShaderModule vertex_shader;
        VkShaderModule fragment_shader;

        vk_try(
            create_shader_module(res.device, (const uint32_t *)VERTEX_SHADER, VERTEX_SHADER_LEN, &vertex_shader),
            "Failed to create vertex shader module"
        );
        vk_try(
            create_shader_module(res.device, (const uint32_t *)FRAGMENT_SHADER, FRAGMENT_SHADER_LEN, &fragment_shader),
            "Failed to create fragment shader module"
        );

        VkPipelineShaderStageCreateInfo vertex_shader_stage_create_info = {0};
        vertex_shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertex_shader_stage_create_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertex_shader_stage_create_info.module = vertex_shader;
        vertex_shader_stage_create_info.pName = "main";

        VkPipelineShaderStageCreateInfo fragment_shader_stage_create_info = {0};
        fragment_shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragment_shader_stage_create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragment_shader_stage_create_info.module = fragment_shader;
        fragment_shader_stage_create_info.pName = "main";

        VkPipelineShaderStageCreateInfo shader_stages[2] = {vertex_shader_stage_create_info, fragment_shader_stage_create_info};

        VkPipelineVertexInputStateCreateInfo vertex_input_state = {0};
        vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input_state.vertexBindingDescriptionCount = 0;
        vertex_input_state.vertexAttributeDescriptionCount = 0;

        VkPipelineInputAssemblyStateCreateInfo input_assembly = {0};
        input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        input_assembly.primitiveRestartEnable = VK_FALSE;

        static const VkDynamicState DYNAMIC_STATES[] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };
        static const uint32_t DYNAMIC_STATES_COUNT = sizeof(DYNAMIC_STATES) / sizeof(VkDynamicState);

        VkPipelineDynamicStateCreateInfo dynamic_state = {0};
        dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state.dynamicStateCount = DYNAMIC_STATES_COUNT;
        dynamic_state.pDynamicStates = DYNAMIC_STATES;

        VkPipelineViewportStateCreateInfo viewport_state = {0};
        viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state.scissorCount = 1;
        viewport_state.viewportCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer_state = {0};
        rasterizer_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer_state.depthClampEnable = VK_FALSE;
        rasterizer_state.rasterizerDiscardEnable = VK_FALSE;
        rasterizer_state.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer_state.lineWidth = 1.0f;
        rasterizer_state.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer_state.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer_state.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisample_state = {0};
        multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample_state.sampleShadingEnable = VK_FALSE;
        multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisample_state.minSampleShading = 1.0f;
        multisample_state.pSampleMask = NULL;
        multisample_state.alphaToCoverageEnable = VK_FALSE;
        multisample_state.alphaToOneEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState color_blend_attachment_state = {0};
        color_blend_attachment_state.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        color_blend_attachment_state.blendEnable = VK_TRUE;
        color_blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        color_blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        color_blend_attachment_state.colorBlendOp = VK_BLEND_OP_ADD;
        color_blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        color_blend_attachment_state.alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo color_blend_state = {0};
        color_blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend_state.logicOpEnable = VK_FALSE;
        color_blend_state.logicOp = VK_LOGIC_OP_COPY;
        color_blend_state.attachmentCount = 1;
        color_blend_state.pAttachments = &color_blend_attachment_state;
        color_blend_state.blendConstants[0] = 0.0f;
        color_blend_state.blendConstants[1] = 0.0f;
        color_blend_state.blendConstants[2] = 0.0f;
        color_blend_state.blendConstants[3] = 0.0f;

        VkPipelineLayoutCreateInfo pipeline_layout_create_info = {0};
        pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_create_info.setLayoutCount = 0;
        pipeline_layout_create_info.pSetLayouts = NULL;
        pipeline_layout_create_info.pushConstantRangeCount = 0;
        pipeline_layout_create_info.pPushConstantRanges = NULL;

        vk_try(
            vkCreatePipelineLayout(res.device, &pipeline_layout_create_info, NULL, &res.pipeline_layout),
            "Failed to create pipeline layout"
        );

        VkGraphicsPipelineCreateInfo create_info = {0};
        create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        create_info.stageCount = 2;
        create_info.pStages = shader_stages;
        create_info.pVertexInputState = &vertex_input_state;
        create_info.pInputAssemblyState = &input_assembly;
        create_info.pViewportState = &viewport_state;
        create_info.pRasterizationState = &rasterizer_state;
        create_info.pMultisampleState = &multisample_state;
        create_info.pDepthStencilState = NULL;
        create_info.pColorBlendState = &color_blend_state;
        create_info.pDynamicState = &dynamic_state;
        create_info.layout = res.pipeline_layout;
        create_info.renderPass = res.render_pass;
        create_info.subpass = 0;
        create_info.basePipelineHandle = VK_NULL_HANDLE;
        create_info.basePipelineIndex = -1;

        vk_try(
            vkCreateGraphicsPipelines(res.device, VK_NULL_HANDLE, 1, &create_info, NULL, &res.graphics_pipeline),
            "Failed to create graphics pipeline"
        );

        vkDestroyShaderModule(res.device, fragment_shader, NULL);
        vkDestroyShaderModule(res.device, vertex_shader, NULL);
    }

    // Framebuffers
    {
        res.framebuffers = (VkFramebufferVec)vec_init();
        vec_grow(&res.framebuffers, res.images.len);

        for (size_t i = 0; i < res.image_views.len; i++) {
            VkImageView attachments[] = {res.image_views.data[i]};

            VkFramebufferCreateInfo create_info = {0};
            create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            create_info.renderPass = res.render_pass;
            create_info.attachmentCount = 1;
            create_info.pAttachments = attachments;
            create_info.width = res.config.extent.width;
            create_info.height = res.config.extent.height;
            create_info.layers = 1;

            vk_try(
                vkCreateFramebuffer(res.device, &create_info, NULL, &res.framebuffers.data[i]),
                "Failed to create framebuffer"
            );
        }

        res.framebuffers.len = res.image_views.len;
    }

    // Command pool
    {
        VkCommandPoolCreateInfo create_info = {0};
        create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        create_info.queueFamilyIndex = res.queue_family_indices.graphics;
        create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        vk_try(vkCreateCommandPool(res.device, &create_info, NULL, &res.command_pool), "Failed to create command pool");
    }

    // Command buffer
    {
        VkCommandBufferAllocateInfo alloc_info = {0};
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.commandPool = res.command_pool;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = 1;

        vk_try(vkAllocateCommandBuffers(res.device, &alloc_info, &res.command_buffer), "Failed to allocate command buffer");
    }

    // Synchronisation
    {
        VkSemaphoreCreateInfo semaphore_create_info = {0};
        semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fence_create_info = {0};
        fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        vk_try(
            vkCreateSemaphore(res.device, &semaphore_create_info, NULL, &res.image_available_semaphore),
            "Failed to create semaphore"
        );
        vk_try(
            vkCreateSemaphore(res.device, &semaphore_create_info, NULL, &res.render_finished_semaphore),
            "Failed to create semaphore"
        );
        vk_try(vkCreateFence(res.device, &fence_create_info, NULL, &res.in_flight_fence), "Failed to create fence");
    }

    vec_drop(required_exts);
    vec_drop(enabled_layers);

    return res;
}

void ctx_record_command_buffer(GraphicContext *ctx, uint32_t image_index) {
    VkCommandBufferBeginInfo begin_info = {0};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    vk_try(vkBeginCommandBuffer(ctx->command_buffer, &begin_info), "Failed to begin command buffer");

    VkClearValue clear_color = (VkClearValue){{{0.0f, 0.0f, 0.0f, 1.0f}}};

    VkRenderPassBeginInfo render_pass_info = {0};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = ctx->render_pass;
    render_pass_info.framebuffer = vec_get(&ctx->framebuffers, image_index);
    render_pass_info.renderArea.offset = (VkOffset2D){0, 0};
    render_pass_info.renderArea.extent = ctx->config.extent;
    render_pass_info.clearValueCount = 1;
    render_pass_info.pClearValues = &clear_color;

    VkViewport viewport = {0};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)ctx->config.extent.width;
    viewport.height = (float)ctx->config.extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {0};
    scissor.offset = (VkOffset2D){0, 0};
    scissor.extent = ctx->config.extent;

    vkCmdBeginRenderPass(ctx->command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(ctx->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->graphics_pipeline);
    vkCmdSetViewport(ctx->command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(ctx->command_buffer, 0, 1, &scissor);
    vkCmdDraw(ctx->command_buffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(ctx->command_buffer);

    vk_try(vkEndCommandBuffer(ctx->command_buffer), "Failed to record command buffer");
}

void ctx_draw_frame(GraphicContext *ctx) {
    vkWaitForFences(ctx->device, 1, &ctx->in_flight_fence, VK_TRUE, UINT64_MAX);
    vkResetFences(ctx->device, 1, &ctx->in_flight_fence);

    uint32_t image_index;
    vkAcquireNextImageKHR(ctx->device, ctx->swapchain, UINT64_MAX, ctx->image_available_semaphore, VK_NULL_HANDLE, &image_index);
    vkResetCommandBuffer(ctx->command_buffer, 0);
    ctx_record_command_buffer(ctx, image_index);

    VkSubmitInfo submit_info = {0};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore semaphores[] = {ctx->image_available_semaphore};
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = semaphores;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &ctx->command_buffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &ctx->render_finished_semaphore;

    vk_try(vkQueueSubmit(ctx->graphics_queue, 1, &submit_info, ctx->in_flight_fence), "Failed to submit draw command buffer");

    VkPresentInfoKHR present_info = {0};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &ctx->render_finished_semaphore;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &ctx->swapchain;
    present_info.pImageIndices = &image_index;
    present_info.pResults = NULL;

    vkQueuePresentKHR(ctx->present_queue, &present_info);
}

void ctx_drop(GraphicContext ctx) {
    vkDeviceWaitIdle(ctx.device);

    vkDestroyFence(ctx.device, ctx.in_flight_fence, NULL);
    vkDestroySemaphore(ctx.device, ctx.render_finished_semaphore, NULL);
    vkDestroySemaphore(ctx.device, ctx.image_available_semaphore, NULL);
    vkDestroyCommandPool(ctx.device, ctx.command_pool, NULL);
    vec_foreach(&ctx.framebuffers, framebuffer, vkDestroyFramebuffer(ctx.device, framebuffer, NULL));
    vkDestroyPipeline(ctx.device, ctx.graphics_pipeline, NULL);
    vkDestroyPipelineLayout(ctx.device, ctx.pipeline_layout, NULL);
    vkDestroyRenderPass(ctx.device, ctx.render_pass, NULL);
    vec_foreach(&ctx.image_views, view, vkDestroyImageView(ctx.device, view, NULL););
    vkDestroySwapchainKHR(ctx.device, ctx.swapchain, NULL);
    vkDestroyDevice(ctx.device, NULL);
    vkDestroySurfaceKHR(ctx.instance, ctx.surface, NULL);
    DestroyDebugUtilsMessengerEXT(ctx.instance, ctx.debug_messenger, NULL);
    vkDestroyInstance(ctx.instance, NULL);

    swapchain_support_details_drop(ctx.swapchain_support);
    vec_drop(ctx.images);
    vec_drop(ctx.image_views);
    vec_drop(ctx.framebuffers);

    log_info("Context destroyed");
}

Window window_init(const char *title, uint32_t width, uint32_t height) {
    static atomic_bool GLFW_INITIALIZED = false;

    assert(!GLFW_INITIALIZED, "GLFW already initialized");

    assert(glfwInit() == GLFW_TRUE, "Failed to initialize GLFW");
    GLFW_INITIALIZED = true;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    Window res;

    res.win = glfwCreateWindow(width, height, title, NULL, NULL);

    log_info("Window created");

    return res;
}

void window_run(Window win, GraphicContext *ctx) {
    while (!glfwWindowShouldClose(win.win)) {
        glfwPollEvents();
        ctx_draw_frame(ctx);
    }
}

void window_drop(Window win) {
    glfwDestroyWindow(win.win);
    glfwTerminate();
    log_info("Window destroyed");
}

int main() {
    logger_set_fd(stdout);
    logger_enable_severities(Info | Warning | Error);
    logger_init();

    Window win = window_init("Window!", 800, 600);
    GraphicContext ctx = ctx_init("vulkan_app", &win);

    window_run(win, &ctx);

    ctx_drop(ctx);
    window_drop(win);
}

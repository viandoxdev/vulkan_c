#define GLFW_INCLUDE_VULKAN
#include "log.h"
#include "proxies.h"
#include "vk_enum_string_helper.h"

#include <GLFW/glfw3.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>

// Basic assertion macro (always checks)
#ifdef LOG_DISABLE
#define assert(c, fmt, ...)                                                                                                      \
    do {                                                                                                                         \
        if (!(c)) {                                                                                                              \
            printf(fmt "\n", __VA_ARGS__);                                                                                       \
            exit(1);                                                                                                             \
        }                                                                                                                        \
    } while (false)
#else // LOG_DISABLE
#define assert(c, ...)                                                                                                           \
    do {                                                                                                                         \
        if (!(c)) {                                                                                                              \
            log_error(__VA_ARGS__);                                                                                              \
            exit(1);                                                                                                             \
        }                                                                                                                        \
    } while (false)
#endif // LOG_DISABLE

// Only check if NDEBUG isn't defined
#ifdef NDEBUG
#define debug_assert(c, ...) (void)0
#else
#define debug_assert(c, ...) assert(c, __VA_ARGS__)
#endif

#define assert_eq(a, b, ...) assert(a == b, __VA_ARGS__)

// Assert allocation succeeded (var != NULL)
#define assert_alloc(var) debug_assert(var != NULL, "Failed to allocate memory for " #var " (Out of memory ?)")

// run expr, and assert that the returned VkResult is VK_SUCCESS
#define vk_try(expr, str)                                                                                                        \
    do {                                                                                                                         \
        VkResult _res = expr;                                                                                                    \
        assert_eq(_res, VK_SUCCESS, str " (%s)", string_VkResult(_res));                                                         \
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
    VkInstance instance;
    // Debug messenger used to route vulkan messages through the logger
    VkDebugUtilsMessengerEXT debug_messenger;
    // Window surface
    VkSurfaceKHR surface;
    VkPhysicalDevice physical_device;
    QueueFamilyIndices queue_family_indices;
    SwapChainSupportDetails swapchain_support;
    VkDevice device;

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

GraphicContext ctx_init(const char *app_name, Window *win) {
    GraphicContext res = {0};

    uint32_t required_exts_count;
    const char **required_exts;
    uint32_t enabled_layers_count = 0;
    const char **enabled_layers;

    // Required extensions
    {
        uint32_t glfw_ext_count;
        const char **glfw_exts = glfwGetRequiredInstanceExtensions(&glfw_ext_count);

        required_exts_count = glfw_ext_count + REQUIRED_EXTENSIONS_COUNT;
        required_exts = malloc(required_exts_count * sizeof(char *));

        assert_alloc(required_exts);

        for (uint32_t i = 0; i < glfw_ext_count; i++) {
            required_exts[i] = glfw_exts[i];
        }
        for (uint32_t i = 0; i < REQUIRED_EXTENSIONS_COUNT; i++) {
            required_exts[i + glfw_ext_count] = REQUIRED_EXTENSIONS[i];
        }
    }

    // Validation layers
#ifdef ENABLE_VALIDATION_LAYERS
    {
        uint32_t supported_layers_count;
        VkLayerProperties *supported_layers;

        vkEnumerateInstanceLayerProperties(&supported_layers_count, NULL);

        supported_layers = malloc(supported_layers_count * sizeof(VkLayerProperties));
        enabled_layers = malloc(VALIDATION_LAYER_COUNT * sizeof(char *));

        assert_alloc(supported_layers);
        assert_alloc(enabled_layers);

        vkEnumerateInstanceLayerProperties(&supported_layers_count, supported_layers);

        for (uint32_t i = 0; i < VALIDATION_LAYER_COUNT; i++) {
            const char *layer_name = VALIDATION_LAYERS[i];
            bool found = false;
            for (uint32_t j = 0; j < supported_layers_count; j++) {
                if (strcmp(layer_name, supported_layers[j].layerName) == 0) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                log_warn("Validation layer '%s' requested, but not available", layer_name);
            } else {
                enabled_layers[enabled_layers_count++] = layer_name;
            }
        }

        free(supported_layers);
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

        // Log the supported extensions
#ifndef LOG_DISABLE
        {
            uint32_t supported_exts_count;
            VkExtensionProperties *supported_exts;

            vkEnumerateInstanceExtensionProperties(NULL, &supported_exts_count, NULL);
            supported_exts = malloc(supported_exts_count * sizeof(VkExtensionProperties));
            assert_alloc(supported_exts);

            vkEnumerateInstanceExtensionProperties(NULL, &supported_exts_count, supported_exts);

            log_info("Availables vulkan extensions:");
            for (uint32_t i = 0; i < supported_exts_count; i++) {
                log_info("    %s", supported_exts[i].extensionName);
            }

            free(supported_exts);
        }
#endif // LOG_DISABLE

        VkDebugUtilsMessengerCreateInfoEXT debug_create_info = debug_messenger_create_info();

        VkInstanceCreateInfo create_info = {0};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.pApplicationInfo = &appinfo;
        create_info.enabledExtensionCount = required_exts_count;
        create_info.ppEnabledExtensionNames = required_exts;
        create_info.pNext = &debug_create_info;

#ifdef ENABLE_VALIDATION_LAYERS
        create_info.enabledLayerCount = enabled_layers_count;
        create_info.ppEnabledLayerNames = enabled_layers;
        log_info("Enabled validation layers:");
        for (uint32_t i = 0; i < enabled_layers_count; i++) {
            log_info("    %s", enabled_layers[i]);
        }
#else
        create_info.enabledLayerCount = 0;
#endif

        vk_try(vkCreateInstance(&create_info, NULL, &res.instance), "Failed to create vulkan instance");

        log_info("Enabled vulkan extensions:");
        for (uint32_t i = 0; i < required_exts_count; i++) {
            log_info("    %s", required_exts[i]);
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

        uint32_t device_count = 0;
        vkEnumeratePhysicalDevices(res.instance, &device_count, NULL);

        if (device_count == 0) {
            log_error("No vulkan device found.");
            exit(1);
        }

        VkPhysicalDevice *devices = malloc(device_count * sizeof(VkPhysicalDevice *));
        assert_alloc(devices);
        vkEnumeratePhysicalDevices(res.instance, &device_count, devices);

        int32_t max_score = -1;
        VkPhysicalDeviceProperties final_device_props;
        log_info("Suitable vulkan devices:");
        for (uint32_t i = 0; i < device_count; i++) {
            VkPhysicalDevice dev = devices[i];
            VkPhysicalDeviceProperties props;
            VkPhysicalDeviceFeatures feats;
            QueueFamilyIndices idx;
            uint32_t device_extensions_count = 0;
            VkExtensionProperties *device_extensions = NULL;

            idx = queue_family_indices_init(dev, res.surface);
            vkGetPhysicalDeviceProperties(dev, &props);
            vkGetPhysicalDeviceFeatures(dev, &feats);
            vkEnumerateDeviceExtensionProperties(dev, NULL, &device_extensions_count, NULL);
            device_extensions = malloc(device_extensions_count * sizeof(VkExtensionProperties));
            assert_alloc(device_extensions);
            vkEnumerateDeviceExtensionProperties(dev, NULL, &device_extensions_count, device_extensions);

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
                    for (int j = 0; j < device_extensions_count; j++) {
                        if (strcmp(ext, device_extensions[j].extensionName) == 0) {
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

                // For some reasons my nvdia gpu doesn't work (crashes, somthing about an invalid surface, probably nvidia linux drivers being faulty).
                // TODO: remove this, and add a way to override device selection with some kind of config file (instead of hardcoding it).
                if (strstr(props.deviceName, "NVIDIA")) {
                    score -= 6;
                }
            }

            log_info("    '%s' (score: %d)", props.deviceName, score);

            if (score > max_score) {
                max_score = score;
                res.physical_device = dev;
                res.queue_family_indices = idx;
                final_device_props = props;
            }

        skip_device:
            free(device_extensions);
        }

        free(devices);

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
        create_info.enabledLayerCount = enabled_layers_count;
        create_info.ppEnabledLayerNames = enabled_layers;
#else
        create_info.enabledLayerCount = 0;
#endif

        vk_try(vkCreateDevice(res.physical_device, &create_info, NULL, &res.device), "Failed to create logical device");

        vkGetDeviceQueue(res.device, res.queue_family_indices.graphics, 0, &res.graphics_queue);
        vkGetDeviceQueue(res.device, res.queue_family_indices.present, 0, &res.present_queue);
    }

    free(required_exts);
    free(enabled_layers);

    return res;
}

void ctx_drop(GraphicContext ctx) {
    swapchain_support_details_drop(ctx.swapchain_support);
    vkDestroyDevice(ctx.device, NULL);
    vkDestroySurfaceKHR(ctx.instance, ctx.surface, NULL);
    DestroyDebugUtilsMessengerEXT(ctx.instance, ctx.debug_messenger, NULL);
    vkDestroyInstance(ctx.instance, NULL);
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

void window_run(Window win) {
    while (!glfwWindowShouldClose(win.win)) {
        glfwPollEvents();
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

    // window_run(win);

    ctx_drop(ctx);
    window_drop(win);
}

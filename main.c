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

#define ENABLE_VALIDATION_LAYERS

#if NDEBUG
#define assert_non_null(VAR) (void)0
#else
#define assert_non_null(VAR)                                                                                                     \
    do {                                                                                                                         \
        if (VAR == NULL) {                                                                                                       \
            log_error("Failed to allocate memory for " #VAR " (Out of memory ?)");                                               \
            exit(1);                                                                                                             \
        }                                                                                                                        \
    } while (false)
#endif

// Structs

typedef struct {
    GLFWwindow *win;
} Window;

typedef struct {
    VkInstance instance;
    VkDebugUtilsMessengerEXT debug_messenger;
    VkPhysicalDevice physical_device;
    // Number of supported extensions
    uint32_t supported_exts_count;
    // Properties of the supported extensions
    VkExtensionProperties *supported_exts;
    // Number of required extensions
    uint32_t required_exts_count;
    // Name of the required extensions
    const char **required_exts;
    // Number of supported validation layers
    uint32_t supported_layers_count;
    // Properties of the supported validation layers
    VkLayerProperties *supported_layers;
    // Number of enabled validation layers. Should be 0 if ENABLE_VALIDATION_LAYERS is undefined
    uint32_t enabled_layers_count;
    // Names of the enabled validation layers
    const char **enabled_layers;
} GraphicContext;

// Vulkan configuration constants

static const char *VALIDATION_LAYERS[] = {"VK_LAYER_KHRONOS_validation"};
static const uint32_t VALIDATION_LAYER_COUNT = sizeof(VALIDATION_LAYERS) / sizeof(char *);

static const char *REQUIRED_EXTENSIONS[] = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
static const uint32_t REQUIRED_EXTENSIONS_COUNT = sizeof(REQUIRED_EXTENSIONS) / sizeof(char *);

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

GraphicContext ctx_init(const char *app_name) {
    GraphicContext res = {0};

    // Required extensions
    {
        uint32_t glfw_ext_count;
        const char **glfw_exts = glfwGetRequiredInstanceExtensions(&glfw_ext_count);

        res.required_exts_count = glfw_ext_count + REQUIRED_EXTENSIONS_COUNT;
        res.required_exts = malloc(res.required_exts_count * sizeof(char *));

        assert_non_null(res.required_exts);

        for (uint32_t i = 0; i < glfw_ext_count; i++) {
            res.required_exts[i] = glfw_exts[i];
        }
        for (uint32_t i = 0; i < REQUIRED_EXTENSIONS_COUNT; i++) {
            res.required_exts[i + glfw_ext_count] = REQUIRED_EXTENSIONS[i];
        }
    }

    // Validation layers
#ifdef ENABLE_VALIDATION_LAYERS
    {
        vkEnumerateInstanceLayerProperties(&res.supported_layers_count, NULL);

        res.supported_layers = malloc(res.supported_layers_count * sizeof(VkLayerProperties));
        res.enabled_layers = malloc(VALIDATION_LAYER_COUNT * sizeof(char *));

        assert_non_null(res.supported_layers);
        assert_non_null(res.enabled_layers);

        vkEnumerateInstanceLayerProperties(&res.supported_layers_count, res.supported_layers);

        const char **enabled_layers = res.enabled_layers;

        for (uint32_t i = 0; i < VALIDATION_LAYER_COUNT; i++) {
            const char *layer_name = VALIDATION_LAYERS[i];
            bool found = false;
            for (uint32_t j = 0; j < res.supported_layers_count; j++) {
                if (strcmp(layer_name, res.supported_layers[j].layerName) == 0) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                log_warn("Validation layer '%s' requested, but not available", layer_name);
            } else {
                *enabled_layers++ = layer_name;
                res.enabled_layers_count++;
            }
        }
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

        vkEnumerateInstanceExtensionProperties(NULL, &res.supported_exts_count, NULL);
        res.supported_exts = malloc(res.supported_exts_count * sizeof(VkExtensionProperties));
        if (res.supported_exts == NULL) {
            log_error("Failed to allocate memory for supported extensions arrays. (OOM ?)");
            exit(1);
        }
        vkEnumerateInstanceExtensionProperties(NULL, &res.supported_exts_count, res.supported_exts);

        log_info("Availables vulkan extensions:");
        for (uint32_t i = 0; i < res.supported_exts_count; i++) {
            log_info("    %s", res.supported_exts[i].extensionName);
        }

        VkDebugUtilsMessengerCreateInfoEXT debug_create_info = debug_messenger_create_info();

        VkInstanceCreateInfo create_info = {0};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.pApplicationInfo = &appinfo;
        create_info.enabledExtensionCount = res.required_exts_count;
        create_info.ppEnabledExtensionNames = res.required_exts;
        create_info.pNext = &debug_create_info;

#ifdef ENABLE_VALIDATION_LAYERS
        create_info.enabledLayerCount = res.enabled_layers_count;
        create_info.ppEnabledLayerNames = res.enabled_layers;
        log_info("Enabled validation layers:");
        for (uint32_t i = 0; i < res.enabled_layers_count; i++) {
            log_info("    %s", res.enabled_layers[i]);
        }
#else
        create_info.enabledLayerCount = 0;
#endif

        VkResult result = vkCreateInstance(&create_info, NULL, &res.instance);
        if (result != VK_SUCCESS) {
            log_error("Failed to create vulkan instance (%s)", string_VkResult(result));
            exit(1);
        }

        log_info("Enabled vulkan extensions:");
        for (uint32_t i = 0; i < res.required_exts_count; i++) {
            log_info("    %s", res.required_exts[i]);
        }
    }

    // Debug messenger
    {
        VkDebugUtilsMessengerCreateInfoEXT create_info = debug_messenger_create_info();
        VkResult result = CreateDebugUtilsMessengerEXT(res.instance, &create_info, NULL, &res.debug_messenger);

        if (result != VK_SUCCESS) {
            log_error("Failed to create debug messenger (%s)", string_VkResult(result));
            exit(1);
        }
    }

    // Physical Device
    {
        res.physical_device = VK_NULL_HANDLE;

        uint32_t device_count = 0;
        vkEnumeratePhysicalDevices(res.instance, &device_count, NULL);

        if(device_count == 0) {
            log_error("No vulkan device found.");
            exit(1);
        }

        VkPhysicalDevice * devices = malloc(device_count * sizeof(VkPhysicalDevice *));
        assert_non_null(devices);
        vkEnumeratePhysicalDevices(res.instance, &device_count, devices);

        uint32_t max_score = -1;
        for(uint32_t i = 0; i < device_count; i++) {
            VkPhysicalDevice * dev = &devices[i];
            VkPhysicalDeviceProperties props;
            VkPhysicalDeviceFeatures feats;

            vkGetPhysicalDeviceProperties(*dev, &props);
            vkGetPhysicalDeviceFeatures(*dev, &feats);

            // Conditions preventing this device from being choosen altogether
            if (false) continue;

            // The device is valid, now compute its score to see if it is the best one.
            uint32_t score = 0;
            {
                // Compute score
                switch(props.deviceType) {
                    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                        score += 10;
                        break;
                    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                        score += 5;
                        break;
                    default:
                        break;
                }
            }

            if (score > max_score) {
                max_score = score;
                res.physical_device = *dev;
            }
        }

        if(res.physical_device == VK_NULL_HANDLE) {
            log_error("Couldn't find suitable vulkan device.");
            exit(1);
        }
    }

    return res;
}

void ctx_drop(GraphicContext ctx) {
    DestroyDebugUtilsMessengerEXT(ctx.instance, ctx.debug_messenger, NULL);
    vkDestroyInstance(ctx.instance, NULL);
}

Window window_init(const char *title, uint32_t width, uint32_t height) {
    static atomic_bool GLFW_INITIALIZED = false;
    if (GLFW_INITIALIZED) {
        log_error("GLFW already initialized");
        exit(1);
    }

    if (glfwInit() != GLFW_TRUE) {
        log_error("Failed to initialize GLFW");
        exit(1);
    }
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
    logger_enable_severities(Debug | Info | Warning | Error);
    logger_init();

    Window win = window_init("Window!", 800, 600);
    GraphicContext ctx = ctx_init("vulkan_app");

    // window_run(win);

    ctx_drop(ctx);
    window_drop(win);
}

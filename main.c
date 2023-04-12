#define GLFW_INCLUDE_VULKAN
#include "log.h"
#include "vk_enum_string_helper.h"

#include <GLFW/glfw3.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>

typedef struct {
    GLFWwindow *win;
} Window;

typedef struct {
    VkInstance instance;

    uint32_t               supported_exts_count;
    VkExtensionProperties *supported_exts;

    uint32_t           supported_layers_count;
    VkLayerProperties *supported_layers;

    uint32_t     enabled_layers_count;
    const char **enabled_layers;
} GraphicContext;

static const char    *VALIDATION_LAYERS[]    = {"VK_LAYER_KHRONOS_validation"};
static const uint32_t VALIDATION_LAYER_COUNT = sizeof(VALIDATION_LAYERS) / sizeof(char *);

GraphicContext ctx_init(const char *app_name) {
    GraphicContext res = {0};

#ifdef ENABLE_VALIDATION_LAYERS
    // Validation layers
    {
        vkEnumerateInstanceLayerProperties(&res.supported_layers_count, NULL);

        res.supported_layers = malloc(res.supported_layers_count * sizeof(VkLayerProperties));
        res.enabled_layers   = malloc(VALIDATION_LAYER_COUNT * sizeof(char *));
        if (res.supported_layers == NULL || res.enabled_layers == NULL) {
            log_error("Failed to allocate memory for validation layers arrays (OOM ?)");
            exit(1);
        }
        vkEnumerateInstanceLayerProperties(&res.supported_layers_count, res.supported_layers);

        const char **enabled_layers = res.enabled_layers;

        for (uint32_t i = 0; i < VALIDATION_LAYER_COUNT; i++) {
            const char *layer_name = VALIDATION_LAYERS[i];
            bool        found      = false;
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
#endif

    // Instance
    {
        VkApplicationInfo appinfo  = {0};
        appinfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appinfo.pApplicationName   = app_name;
        appinfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appinfo.pEngineName        = "None";
        appinfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
        appinfo.apiVersion         = VK_API_VERSION_1_0;

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

        uint32_t     glfw_ext_count;
        const char **glfw_exts = glfwGetRequiredInstanceExtensions(&glfw_ext_count);

        VkInstanceCreateInfo create_info    = {0};
        create_info.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.pApplicationInfo        = &appinfo;
        create_info.enabledExtensionCount   = glfw_ext_count;
        create_info.ppEnabledExtensionNames = glfw_exts;

#ifdef ENABLE_VALIDATION_LAYERS
        create_info.enabledLayerCount = res.enabled_layers_count;
        create_info.ppEnabledLayerNames = res.enabled_layers;
        log_info("Enabled validation layers:");
        for(uint32_t i = 0; i < res.enabled_layers_count; i++) {
            log_info("    %s", res.enabled_layers[i]);
        }
#else
        create_info.enabledLayerCount       = 0;
#endif

        VkResult ins_res = vkCreateInstance(&create_info, NULL, &res.instance);
        if (ins_res != VK_SUCCESS) {
            log_error("Failed to create vulkan instance (%s)", string_VkResult(ins_res));
            exit(1);
        }
    }

    return res;
}

void ctx_drop(GraphicContext ctx) { vkDestroyInstance(ctx.instance, NULL); }

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

    Window         win = window_init("Window!", 800, 600);
    GraphicContext ctx = ctx_init("vulkan_app");

    window_run(win);

    ctx_drop(ctx);
    window_drop(win);
}

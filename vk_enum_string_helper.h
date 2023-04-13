#ifndef VK_ENUM_STRING_HELPER_H
#define VK_ENUM_STRING_HELPER_H

#include <vulkan/vulkan.h>

// just extracted string_VkResult from the original header

static inline const char* string_VkResult(VkResult input_value)
{
    switch (input_value)
    {
        case VK_ERROR_COMPRESSION_EXHAUSTED_EXT:
            return "VK_ERROR_COMPRESSION_EXHAUSTED_EXT";
        case VK_ERROR_DEVICE_LOST:
            return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_EXTENSION_NOT_PRESENT:
            return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT:
            return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_FORMAT_NOT_SUPPORTED:
            return "VK_ERROR_FORMAT_NOT_SUPPORTED";
        case VK_ERROR_FRAGMENTATION:
            return "VK_ERROR_FRAGMENTATION";
        case VK_ERROR_FRAGMENTED_POOL:
            return "VK_ERROR_FRAGMENTED_POOL";
        case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
            return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
        case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR:
            return "VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR";
        case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
            return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
        case VK_ERROR_INCOMPATIBLE_DRIVER:
            return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_INITIALIZATION_FAILED:
            return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT:
            return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
        case VK_ERROR_INVALID_EXTERNAL_HANDLE:
            return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
        case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:
            return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
        case VK_ERROR_INVALID_SHADER_NV:
            return "VK_ERROR_INVALID_SHADER_NV";
#ifdef VK_ENABLE_BETA_EXTENSIONS
        case VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR:
            return "VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR";
#endif // VK_ENABLE_BETA_EXTENSIONS
        case VK_ERROR_LAYER_NOT_PRESENT:
            return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_MEMORY_MAP_FAILED:
            return "VK_ERROR_MEMORY_MAP_FAILED";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
            return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
        case VK_ERROR_NOT_PERMITTED_KHR:
            return "VK_ERROR_NOT_PERMITTED_KHR";
        case VK_ERROR_OUT_OF_DATE_KHR:
            return "VK_ERROR_OUT_OF_DATE_KHR";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_POOL_MEMORY:
            return "VK_ERROR_OUT_OF_POOL_MEMORY";
        case VK_ERROR_SURFACE_LOST_KHR:
            return "VK_ERROR_SURFACE_LOST_KHR";
        case VK_ERROR_TOO_MANY_OBJECTS:
            return "VK_ERROR_TOO_MANY_OBJECTS";
        case VK_ERROR_UNKNOWN:
            return "VK_ERROR_UNKNOWN";
        case VK_ERROR_VALIDATION_FAILED_EXT:
            return "VK_ERROR_VALIDATION_FAILED_EXT";
        case VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR:
            return "VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR";
        case VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR:
            return "VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR";
        case VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR:
            return "VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR";
        case VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR:
            return "VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR";
        case VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR:
            return "VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR";
        case VK_EVENT_RESET:
            return "VK_EVENT_RESET";
        case VK_EVENT_SET:
            return "VK_EVENT_SET";
        case VK_INCOMPLETE:
            return "VK_INCOMPLETE";
        case VK_NOT_READY:
            return "VK_NOT_READY";
        case VK_OPERATION_DEFERRED_KHR:
            return "VK_OPERATION_DEFERRED_KHR";
        case VK_OPERATION_NOT_DEFERRED_KHR:
            return "VK_OPERATION_NOT_DEFERRED_KHR";
        case VK_PIPELINE_COMPILE_REQUIRED:
            return "VK_PIPELINE_COMPILE_REQUIRED";
        case VK_SUBOPTIMAL_KHR:
            return "VK_SUBOPTIMAL_KHR";
        case VK_SUCCESS:
            return "VK_SUCCESS";
        case VK_THREAD_DONE_KHR:
            return "VK_THREAD_DONE_KHR";
        case VK_THREAD_IDLE_KHR:
            return "VK_THREAD_IDLE_KHR";
        case VK_TIMEOUT:
            return "VK_TIMEOUT";
        default:
            return "Unhandled VkResult";
    }
}

static inline const char* string_VkObjectType(VkObjectType input_value)
{
    switch (input_value)
    {
        case VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR:
            return "acceleration_structure";
        case VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_NV:
            return "acceleration_structure";
        case VK_OBJECT_TYPE_BUFFER:
            return "buffer";
        case VK_OBJECT_TYPE_BUFFER_COLLECTION_FUCHSIA:
            return "buffer_collection";
        case VK_OBJECT_TYPE_BUFFER_VIEW:
            return "buffer_view";
        case VK_OBJECT_TYPE_COMMAND_BUFFER:
            return "command_buffer";
        case VK_OBJECT_TYPE_COMMAND_POOL:
            return "command_pool";
        case VK_OBJECT_TYPE_CU_FUNCTION_NVX:
            return "cu_function";
        case VK_OBJECT_TYPE_CU_MODULE_NVX:
            return "cu_module";
        case VK_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT:
            return "debug_report_callback";
        case VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT:
            return "debug_utils_messenger";
        case VK_OBJECT_TYPE_DEFERRED_OPERATION_KHR:
            return "deferred_operation";
        case VK_OBJECT_TYPE_DESCRIPTOR_POOL:
            return "descriptor_pool";
        case VK_OBJECT_TYPE_DESCRIPTOR_SET:
            return "descriptor_set";
        case VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT:
            return "descriptor_set_layout";
        case VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE:
            return "descriptor_update_template";
        case VK_OBJECT_TYPE_DEVICE:
            return "device";
        case VK_OBJECT_TYPE_DEVICE_MEMORY:
            return "device_memory";
        case VK_OBJECT_TYPE_DISPLAY_KHR:
            return "display";
        case VK_OBJECT_TYPE_DISPLAY_MODE_KHR:
            return "display_mode";
        case VK_OBJECT_TYPE_EVENT:
            return "event";
        case VK_OBJECT_TYPE_FENCE:
            return "fence";
        case VK_OBJECT_TYPE_FRAMEBUFFER:
            return "framebuffer";
        case VK_OBJECT_TYPE_IMAGE:
            return "image";
        case VK_OBJECT_TYPE_IMAGE_VIEW:
            return "image_view";
        case VK_OBJECT_TYPE_INDIRECT_COMMANDS_LAYOUT_NV:
            return "indirect_commands_layout";
        case VK_OBJECT_TYPE_INSTANCE:
            return "instance";
        case VK_OBJECT_TYPE_MICROMAP_EXT:
            return "micromap";
        case VK_OBJECT_TYPE_OPTICAL_FLOW_SESSION_NV:
            return "optical_flow_session";
        case VK_OBJECT_TYPE_PERFORMANCE_CONFIGURATION_INTEL:
            return "performance_configuration_intel";
        case VK_OBJECT_TYPE_PHYSICAL_DEVICE:
            return "physical_device";
        case VK_OBJECT_TYPE_PIPELINE:
            return "pipeline";
        case VK_OBJECT_TYPE_PIPELINE_CACHE:
            return "pipeline_cache";
        case VK_OBJECT_TYPE_PIPELINE_LAYOUT:
            return "pipeline_layout";
        case VK_OBJECT_TYPE_PRIVATE_DATA_SLOT:
            return "private_data_slot";
        case VK_OBJECT_TYPE_QUERY_POOL:
            return "query_pool";
        case VK_OBJECT_TYPE_QUEUE:
            return "queue";
        case VK_OBJECT_TYPE_RENDER_PASS:
            return "render_pass";
        case VK_OBJECT_TYPE_SAMPLER:
            return "sampler";
        case VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION:
            return "sampler_ycbcr_conversion";
        case VK_OBJECT_TYPE_SEMAPHORE:
            return "semaphore";
        case VK_OBJECT_TYPE_SHADER_MODULE:
            return "shader_module";
        case VK_OBJECT_TYPE_SURFACE_KHR:
            return "surface";
        case VK_OBJECT_TYPE_SWAPCHAIN_KHR:
            return "swapchain";
        case VK_OBJECT_TYPE_UNKNOWN:
            return "unknown";
        case VK_OBJECT_TYPE_VALIDATION_CACHE_EXT:
            return "validation_cache";
        case VK_OBJECT_TYPE_VIDEO_SESSION_KHR:
            return "video_session";
        case VK_OBJECT_TYPE_VIDEO_SESSION_PARAMETERS_KHR:
            return "video_session_parameters";
        default:
            return "Unhandled VkObjectType";
    }
}

#endif

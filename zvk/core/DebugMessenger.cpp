#include "DebugMessenger.h"

#include <sstream>

NAMESPACE_BEGIN(zvk)

const char* getObjectTypeString(VkObjectType type) {
	switch (type) {
	case VK_OBJECT_TYPE_INSTANCE:
		return "VkInstance";
	case VK_OBJECT_TYPE_PHYSICAL_DEVICE:
		return "VkPhysicalDevice";
	case VK_OBJECT_TYPE_DEVICE:
		return "VkDevice";
	case VK_OBJECT_TYPE_QUEUE:
		return "VkQueue";
	case VK_OBJECT_TYPE_SEMAPHORE:
		return "VkSemaphore";
	case VK_OBJECT_TYPE_COMMAND_BUFFER:
		return "VkCommandBuffer";
	case VK_OBJECT_TYPE_FENCE:
		return "VkFence";
	case VK_OBJECT_TYPE_DEVICE_MEMORY:
		return "VkDeviceMemory";
	case VK_OBJECT_TYPE_BUFFER:
		return "VkBuffer";
	case VK_OBJECT_TYPE_IMAGE:
		return "VkImage";
	case VK_OBJECT_TYPE_EVENT:
		return "VkEvent";
	case VK_OBJECT_TYPE_QUERY_POOL:
		return "VkQueryPool";
	case VK_OBJECT_TYPE_BUFFER_VIEW:
		return "VkBufferView";
	case VK_OBJECT_TYPE_IMAGE_VIEW:
		return "VkImageView";
	case VK_OBJECT_TYPE_SHADER_MODULE:
		return "VkShaderModule";
	case VK_OBJECT_TYPE_PIPELINE_CACHE:
		return "VkPipelineCache";
	case VK_OBJECT_TYPE_PIPELINE_LAYOUT:
		return "VkPipelineLayout";
	case VK_OBJECT_TYPE_RENDER_PASS:
		return "VkRenderPass";
	case VK_OBJECT_TYPE_PIPELINE:
		return "VkPipeline";
	case VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT:
		return "VkDescriptorSetLayout";
	case VK_OBJECT_TYPE_SAMPLER:
		return "VkSampler";
	case VK_OBJECT_TYPE_DESCRIPTOR_POOL:
		return "VkDescriptorPool";
	case VK_OBJECT_TYPE_DESCRIPTOR_SET:
		return "VkDescriptorSet";
	case VK_OBJECT_TYPE_FRAMEBUFFER:
		return "VkFrameBuffer";
	case VK_OBJECT_TYPE_COMMAND_POOL:
		return "VkCommandPool";
	case VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION:
		return "VkSamplerYcbcrConversion";
	case VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE:
		return "VkDescriptorUpdateTemplate";
	case VK_OBJECT_TYPE_PRIVATE_DATA_SLOT:
		return "VkPrivateDataSlot";
	case VK_OBJECT_TYPE_SURFACE_KHR:
		return "VkSurfaceKHR";
	case VK_OBJECT_TYPE_SWAPCHAIN_KHR:
		return "VkSwapchainKHR";
	case VK_OBJECT_TYPE_DISPLAY_KHR:
		return "VkDisplayKHR";
	case VK_OBJECT_TYPE_DISPLAY_MODE_KHR:
		return "VkDisplayModeKHR";
	case VK_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT:
		return "VkDebugReportCallbackEXT";
	case VK_OBJECT_TYPE_VIDEO_SESSION_KHR:
		return "VkVideoSessionKHR";
	case VK_OBJECT_TYPE_VIDEO_SESSION_PARAMETERS_KHR:
		return "VkVideoSessionParametersKHR";
	case VK_OBJECT_TYPE_CU_MODULE_NVX:
		return "VkCuModuleNVX";
	case VK_OBJECT_TYPE_CU_FUNCTION_NVX:
		return "VkCuModuleNVX";
	case VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT:
		return "VkDebugUtilsMessengerEXT";
	case VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR:
		return "VkAccelerationStructureKHR";
	case VK_OBJECT_TYPE_VALIDATION_CACHE_EXT:
		return "VkValidationCacheEXT";
	case VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_NV:
		return "VkAccelerationStructureNV";
	case VK_OBJECT_TYPE_PERFORMANCE_CONFIGURATION_INTEL:
		return "VkPerformanceConfigurationINTEL";
	case VK_OBJECT_TYPE_DEFERRED_OPERATION_KHR:
		return "VkDeferredOperationKHR";
	case VK_OBJECT_TYPE_INDIRECT_COMMANDS_LAYOUT_NV:
		return "VkIndirectCommandsLayoutNV";
	case VK_OBJECT_TYPE_BUFFER_COLLECTION_FUCHSIA:
		return "VkBufferCollectionFUCHSIA";
	case VK_OBJECT_TYPE_MICROMAP_EXT:
		return "VkMicromapEXT";
	case VK_OBJECT_TYPE_OPTICAL_FLOW_SESSION_NV:
		return "VkOpticalFlowSessionNV";
	case VK_OBJECT_TYPE_SHADER_EXT:
		return "VkShaderEXT";
	};
	return "VkUnknown";
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugMessengerCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
	void* pUserData
) {
	char prefix[64];
	char* message = (char*)malloc(strlen(callbackData->pMessage) + 500);
	assert(message);

	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
		strcpy(prefix, "[VERBOSE");
	}
	else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
		strcpy(prefix, "[INFO");
	}
	else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
		strcpy(prefix, "[WARNING");
	}
	else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
		strcpy(prefix, "[ERROR");
	}
	if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) {
		strcat(prefix, "- GENERAL");
	}
	else if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
		strcat(prefix, "- PERF");
	}

	sprintf(message,
		"%s : Message ID Number = %d, Message ID String = %s]\n\t[Message]\n%s",
		prefix,
		callbackData->messageIdNumber,
		callbackData->pMessageIdName,
		callbackData->pMessage
	);

	if (callbackData->objectCount > 0) {
		char tmp_message[500];
		sprintf(tmp_message, "\n\t[Objects : Count = %d]\n", callbackData->objectCount);
		strcat(message, tmp_message);

		for (uint32_t object = 0; object < callbackData->objectCount; ++object) {
			sprintf(tmp_message,
				"\t\t[Object[%d] : Type = %s, Handle = 0x%016x, Name = \"%s\"]\n",
				object,
				getObjectTypeString(callbackData->pObjects[object].objectType),
				callbackData->pObjects[object].objectHandle,
				callbackData->pObjects[object].pObjectName
			);
			strcat(message, tmp_message);
		}
	}
	if (callbackData->cmdBufLabelCount > 0) {
		char tmp_message[500];

		sprintf(tmp_message,
			"\n\t[Command Buffer Labels : Count = %d]\n",
			callbackData->cmdBufLabelCount
		);
		strcat(message, tmp_message);
		for (uint32_t label = 0; label < callbackData->cmdBufLabelCount; ++label) {

			sprintf(tmp_message,
				"\t\t[Label[%d] : %s { %f, %f, %f, %f }]\n",
				label,
				callbackData->pCmdBufLabels[label].pLabelName,
				callbackData->pCmdBufLabels[label].color[0],
				callbackData->pCmdBufLabels[label].color[1],
				callbackData->pCmdBufLabels[label].color[2],
				callbackData->pCmdBufLabels[label].color[3]
			);
			strcat(message, tmp_message);
		}
	}
	printf("%s\n", message);
	fflush(stdout);
	free(message);
	return false;
}

NAMESPACE_END(zvk)

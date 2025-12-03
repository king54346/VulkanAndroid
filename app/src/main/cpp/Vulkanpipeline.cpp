//
// Created by 31483 on 2025/11/29.
//
#include "VulkanJNI.h"
#include <vector>

using namespace VulkanJNI;

// ============================================
// Pipeline Layout
// ============================================
extern "C" JNIEXPORT jlong JNICALL
Java_com_genymobile_scrcpy_vulkan_SimpleVulkanFilter_nativeCreatePipelineLayout(
        JNIEnv* env, jobject /* this */,
        jlong deviceHandle,
        jlong descriptorSetLayoutHandle) {

    VkDevice device = getDevice(deviceHandle);
    VkDescriptorSetLayout descriptorSetLayout = fromHandle<VkDescriptorSetLayout>(descriptorSetLayoutHandle);

    if (!validateHandle(device, "device") || !validateHandle(descriptorSetLayout, "descriptorSetLayout")) {
        return 0;
    }

    // Push constant range for fragment shader
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(float) * 16;  // 4x4 matrix or custom data

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    VkPipelineLayout pipelineLayout;
    VkResult result = vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout);

    if (!validateResult(result, "vkCreatePipelineLayout")) return 0;

    LOGI("✓ Pipeline layout created: %p", (void*)pipelineLayout);
    return toHandle(pipelineLayout);
}

// ============================================
// Graphics Pipeline
// ============================================
namespace {

    struct PipelineConfig {
        VkDevice device;
        VkRenderPass renderPass;
        VkPipelineLayout pipelineLayout;
        VkShaderModule vertShaderModule;
        VkShaderModule fragShaderModule;
    };

    VkPipelineShaderStageCreateInfo createShaderStage(VkShaderStageFlagBits stage, VkShaderModule module) {
        VkPipelineShaderStageCreateInfo stageInfo{};
        stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageInfo.stage = stage;
        stageInfo.module = module;
        stageInfo.pName = "main";
        return stageInfo;
    }

    VkPipelineVertexInputStateCreateInfo createVertexInputState() {
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.pVertexBindingDescriptions = nullptr;
        vertexInputInfo.vertexAttributeDescriptionCount = 0;
        vertexInputInfo.pVertexAttributeDescriptions = nullptr;
        return vertexInputInfo;
    }

    VkPipelineInputAssemblyStateCreateInfo createInputAssemblyState() {
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;
        return inputAssembly;
    }

    VkPipelineViewportStateCreateInfo createViewportState() {
        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;
        // Dynamic state - actual viewport/scissor set at render time
        return viewportState;
    }

    VkPipelineRasterizationStateCreateInfo createRasterizationState() {
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;
        return rasterizer;
    }

    VkPipelineMultisampleStateCreateInfo createMultisampleState() {
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        return multisampling;
    }

    VkPipelineDepthStencilStateCreateInfo createDepthStencilState() {
        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_FALSE;
        depthStencil.depthWriteEnable = VK_FALSE;
        depthStencil.stencilTestEnable = VK_FALSE;
        return depthStencil;
    }

    VkPipelineColorBlendStateCreateInfo createColorBlendState(VkPipelineColorBlendAttachmentState* attachment) {
        attachment->colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        attachment->blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = attachment;
        return colorBlending;
    }

    VkPipelineDynamicStateCreateInfo createDynamicState(std::vector<VkDynamicState>& dynamicStates) {
        dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();
        return dynamicState;
    }

} // anonymous namespace

extern "C" JNIEXPORT jlong JNICALL
Java_com_genymobile_scrcpy_vulkan_SimpleVulkanFilter_nativeCreateGraphicsPipeline(
        JNIEnv* env, jobject /* this */,
        jlong deviceHandle,
        jlong renderPassHandle,
        jlong pipelineLayoutHandle,
        jlong vertShaderModuleHandle,
        jlong fragShaderModuleHandle) {

    PipelineConfig config{};
    config.device = getDevice(deviceHandle);
    config.renderPass = fromHandle<VkRenderPass>(renderPassHandle);
    config.pipelineLayout = fromHandle<VkPipelineLayout>(pipelineLayoutHandle);
    config.vertShaderModule = fromHandle<VkShaderModule>(vertShaderModuleHandle);
    config.fragShaderModule = fromHandle<VkShaderModule>(fragShaderModuleHandle);

    if (!validateHandle(config.device, "device") ||
        !validateHandle(config.renderPass, "renderPass") ||
        !validateHandle(config.pipelineLayout, "pipelineLayout") ||
        !validateHandle(config.vertShaderModule, "vertShaderModule") ||
        !validateHandle(config.fragShaderModule, "fragShaderModule")) {
        return 0;
    }

    LOGI("=== Creating Graphics Pipeline ===");

    // Shader stages
    VkPipelineShaderStageCreateInfo shaderStages[] = {
            createShaderStage(VK_SHADER_STAGE_VERTEX_BIT, config.vertShaderModule),
            createShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, config.fragShaderModule)
    };

    // Fixed-function pipeline states
    auto vertexInputInfo = createVertexInputState();
    auto inputAssembly = createInputAssemblyState();
    auto viewportState = createViewportState();
    auto rasterizer = createRasterizationState();
    auto multisampling = createMultisampleState();
    auto depthStencil = createDepthStencilState();

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    auto colorBlending = createColorBlendState(&colorBlendAttachment);

    std::vector<VkDynamicState> dynamicStatesVec;
    auto dynamicState = createDynamicState(dynamicStatesVec);

    // Create pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = config.pipelineLayout;
    pipelineInfo.renderPass = config.renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    VkPipeline graphicsPipeline;
    VkResult result = vkCreateGraphicsPipelines(
            config.device,
            VK_NULL_HANDLE,
            1,
            &pipelineInfo,
            nullptr,
            &graphicsPipeline
    );

    if (!validateResult(result, "vkCreateGraphicsPipelines")) return 0;

    LOGI("✓ Graphics pipeline created: %p", (void*)graphicsPipeline);
    return toHandle(graphicsPipeline);
}

// ============================================
// Cleanup Functions
// ============================================
extern "C" JNIEXPORT void JNICALL
Java_com_genymobile_scrcpy_vulkan_SimpleVulkanFilter_nativeDestroyPipeline(
        JNIEnv* env, jobject /* this */, jlong deviceHandle, jlong pipelineHandle) {

    VkDevice device = getDevice(deviceHandle);
    VkPipeline pipeline = fromHandle<VkPipeline>(pipelineHandle);

    if (validateHandle(device, "device") && validateHandle(pipeline, "pipeline")) {
        vkDestroyPipeline(device, pipeline, nullptr);
        LOGD("✓ Pipeline destroyed");
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_genymobile_scrcpy_vulkan_SimpleVulkanFilter_nativeDestroyPipelineLayout(
        JNIEnv* env, jobject /* this */, jlong deviceHandle, jlong pipelineLayoutHandle) {

    VkDevice device = getDevice(deviceHandle);
    VkPipelineLayout pipelineLayout = fromHandle<VkPipelineLayout>(pipelineLayoutHandle);

    if (validateHandle(device, "device") && validateHandle(pipelineLayout, "pipelineLayout")) {
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        LOGD("✓ Pipeline layout destroyed");
    }
}
#include "vulkanexamplebase.h"

#define VERTEX_BUFFER_BIND_ID 0
#define ENABLE_VALIDATION false
#define PARTICLE_COUNT 200

class VulkanExample : public VulkanExampleBase
{
public:
	bool attach = false;
	
	struct Particle
	{
		glm::vec2 pos;
		glm::vec3 color;
	};
	VkVertexInputBindingDescription bindingDescription[1] = {
		vks::initializers::vertexInputBindingDescription(
			0,
			sizeof(Particle),
			VK_VERTEX_INPUT_RATE_VERTEX)
	};
	VkVertexInputAttributeDescription attributeDescription[2] = {
		vks::initializers::vertexInputAttributeDescription(
			0,
			0,
			VK_FORMAT_R32G32_SFLOAT,
			offsetof(Particle, pos)),
		vks::initializers::vertexInputAttributeDescription(
			0,
			1,
			VK_FORMAT_R32G32B32_SFLOAT,
			offsetof(Particle, color))
	};
	vks::Buffer storageBuffer;

	VkPipelineLayout graphicsPipeLayout;
	VkPipeline graphicsPipe;
	
	VulkanExample() : VulkanExampleBase(ENABLE_VALIDATION)
	{
		title = "Compute Shader Sync Test";
		settings.overlay = true;
	}
	~VulkanExample() override
	{
		vkDestroyBuffer(device, storageBuffer.buffer, nullptr);
		vkDestroyPipelineLayout(device, graphicsPipeLayout, nullptr);
	}

	void createStorageBuffer()
	{
		std::default_random_engine            rndEngine(benchmark.active ? 0 : (unsigned) time(nullptr));
		std::uniform_real_distribution<float> rndDist(-1.0f, 1.0f);
 
		// Initial particle positions
		std::vector<Particle> particleBuffer(PARTICLE_COUNT);
		size_t count = 0;
		for (auto &particle : particleBuffer)
		{
			particle.pos = glm::vec2(rndDist(rndEngine), rndDist(rndEngine));
			particle.color = glm::vec3((float)count / PARTICLE_COUNT, (float)count / PARTICLE_COUNT, 1.f);
			
			count++;
		}
 
		VkDeviceSize storageBufferSize = particleBuffer.size() * sizeof(Particle);
 
		// Staging
		// SSBO won't be changed on the host after upload so copy to device local memory
 
		vks::Buffer stagingBuffer;
 
		vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&stagingBuffer,
			storageBufferSize,
			particleBuffer.data());
 
		vulkanDevice->createBuffer(
			// The SSBO will be used as a storage buffer for the compute pipeline and as a vertex buffer in the graphics pipeline
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			&storageBuffer,
			storageBufferSize);
 
		// Copy from staging buffer to storage buffer
		VkCommandBuffer copyCmd    = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		VkBufferCopy    copyRegion = {};
		copyRegion.size            = storageBufferSize;
		vkCmdCopyBuffer(copyCmd, stagingBuffer.buffer, storageBuffer.buffer, 1, &copyRegion);
		vulkanDevice->flushCommandBuffer(copyCmd, queue, true);
 
		stagingBuffer.destroy();
	}

	void createGraphicsPipeline()
	{
		//-----------------------------------
		auto vertShaderModule = vks::tools::loadShader("", device);
		auto fragShaderModule = vks::tools::loadShader("", device);

		VkPipelineShaderStageCreateInfo vertStageInfo = {};
		vertStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertStageInfo.module = vertShaderModule;
		vertStageInfo.pName = "main";
		
		VkPipelineShaderStageCreateInfo fragStageInfo = {};
		fragStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragStageInfo.module = fragShaderModule;
		fragStageInfo.pName = "main";
		
		VkPipelineShaderStageCreateInfo shaderStages[] = { vertStageInfo, fragStageInfo };
		
		//-----------------------------------
		auto vertexInputInfo = vks::initializers::pipelineVertexInputStateCreateInfo();
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.pVertexBindingDescriptions = bindingDescription;
		vertexInputInfo.vertexAttributeDescriptionCount = 2;
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescription;

		//-----------------------------------
		auto inputAssemblyInfo = vks::initializers::pipelineInputAssemblyStateCreateInfo(
			VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
			0,
			VK_FALSE);
		
		//-----------------------------------
		auto viewport = vks::initializers::viewport((float)width, (float)height, 0.0, 1.0);
		auto scissors = vks::initializers::rect2D(width, height, 0, 0);

		auto viewportInfo = vks::initializers::pipelineViewportStateCreateInfo(1, 1);
		viewportInfo.pViewports = &viewport;
		viewportInfo.pScissors = &scissors;

		//-----------------------------------
		auto rasterInfo = vks::initializers::pipelineRasterizationStateCreateInfo(
			VK_POLYGON_MODE_FILL,
			VK_CULL_MODE_NONE,
			VK_FRONT_FACE_COUNTER_CLOCKWISE);
		rasterInfo.depthClampEnable = VK_FALSE;
		rasterInfo.rasterizerDiscardEnable = VK_FALSE;
		rasterInfo.depthBiasEnable = VK_FALSE;
		rasterInfo.depthBiasConstantFactor = 0.0f;
		rasterInfo.depthBiasClamp = 0.0f;
		rasterInfo.depthBiasSlopeFactor = 0.0f;

		//-----------------------------------
		auto multisampleInfo = vks::initializers::pipelineMultisampleStateCreateInfo(
			VK_SAMPLE_COUNT_1_BIT);
		multisampleInfo.sampleShadingEnable = VK_FALSE;
		multisampleInfo.minSampleShading = 1.0f;
		multisampleInfo.pSampleMask = nullptr;
		multisampleInfo.alphaToCoverageEnable = VK_FALSE;
		multisampleInfo.alphaToOneEnable = VK_FALSE;

		//-----------------------------------
		auto colorBlendAttachInfo = vks::initializers::pipelineColorBlendAttachmentState(
			VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
			VK_FALSE);
		colorBlendAttachInfo.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachInfo.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachInfo.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachInfo.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachInfo.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachInfo.alphaBlendOp = VK_BLEND_OP_ADD;
		
		auto colorBlendInfo = vks::initializers::pipelineColorBlendStateCreateInfo(
			1,
			&colorBlendAttachInfo);
		colorBlendInfo.logicOpEnable = VK_FALSE;
		colorBlendInfo.logicOp = VK_LOGIC_OP_COPY;
		colorBlendInfo.blendConstants[0] = 0.0f;
		colorBlendInfo.blendConstants[1] = 0.0f;
		colorBlendInfo.blendConstants[2] = 0.0f;
		colorBlendInfo.blendConstants[3] = 0.0f;

		//-----------------------------------
		VkDynamicState dynamicStates[] = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_LINE_WIDTH
		};
		auto dynamicInfo = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStates, 2);

		//-----------------------------------
		auto pipeLayoutInfo = vks::initializers::pipelineLayoutCreateInfo(nullptr, 0);
		pipeLayoutInfo.pushConstantRangeCount = 0;
		pipeLayoutInfo.pPushConstantRanges = nullptr;
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipeLayoutInfo, nullptr, &graphicsPipeLayout));
		
		//TODO: RenderPass
		//TODO: Set Stages Info
		auto pipeInfo = vks::initializers::pipelineCreateInfo(graphicsPipeLayout, 0);
		VK_CHECK_RESULT( vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &graphicsPipe));
		
		//-----------------------------------
		vkDestroyShaderModule(device, fragShaderModule, nullptr);
		vkDestroyShaderModule(device, vertShaderModule, nullptr);
	}
	
	void prepare() override
	{
		createStorageBuffer();
		createGraphicsPipeline();
		prepared = true;
	}

	void render() override
	{
		if (!prepared) return;
		draw();
	}

	void draw()
	{
		VulkanExampleBase::prepareFrame();

		VkPipelineStageFlags graphicsWaitStageMask[] = {
			 VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
			 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT 
		};
		
		VkSemaphore graphicsWaitSemaphores[] = { semaphores.presentComplete };
		VkSemaphore graphicsSignalSemaphores[] = { semaphores.renderComplete };

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = graphicsWaitSemaphores;
		submitInfo.pWaitDstStageMask = graphicsWaitStageMask;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = graphicsSignalSemaphores;
		VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));

		VulkanExampleBase::submitFrame();
	}

	void OnUpdateUIOverlay(vks::UIOverlay *overlay) override
	{
		if (overlay->header("Settings"))
		{
			overlay->checkBox("Attach?", &attach);
		}
	}
};

VulkanExample *vulkanExample;																		\
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)						\
{																									\
	if (vulkanExample != NULL)																		\
	{																								\
		vulkanExample->handleMessages(hWnd, uMsg, wParam, lParam);									\
	}																								\
	return (DefWindowProc(hWnd, uMsg, wParam, lParam));												\
}																									\
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)									\
{																									\
	for (int32_t i = 0; i < __argc; i++) { VulkanExample::args.push_back(__argv[i]); };  			\
	vulkanExample = new VulkanExample();															\
	vulkanExample->initVulkan();																	\
	vulkanExample->setupWindow(hInstance, WndProc);													\
	vulkanExample->prepare();																		\
	vulkanExample->renderLoop();																	\
	delete(vulkanExample);																			\
	return 0;																						\
}

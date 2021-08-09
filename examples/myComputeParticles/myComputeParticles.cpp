#include "vulkanexamplebase.h"

#define VERTEX_BUFFER_BIND_ID 0
#define ENABLE_VALIDATION false
#define PARTICLE_COUNT 256*1024

class VulkanExample : public VulkanExampleBase
{
public:
	vks::Buffer storageBuffer;
	vks::Buffer uniBuf;

	struct
	{
		bool attach = false;
	} uiSettings;
	
	struct
	{
		VkPipelineLayout pipeLayout;
		VkPipeline pipe;
		VkRenderPass renderPass;
	} graphics;

	struct
	{
		glm::vec4 timeVals;
	} uniforms;
	
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
	
	VulkanExample() : VulkanExampleBase(ENABLE_VALIDATION)
	{
		title = "Compute Shader Sync Test";
		settings.overlay = true;

		uiSettings = {};
		graphics = {};
		uniforms = {};
	}
	~VulkanExample() override
	{
		vkDestroyBuffer(device, storageBuffer.buffer, nullptr);
		
		vkDestroyRenderPass(device, graphics.renderPass, nullptr);
		vkDestroyPipeline(device, graphics.pipe, nullptr);
		vkDestroyPipelineLayout(device, graphics.pipeLayout, nullptr);
	}

	void createStorageBuffer()
	{
		std::default_random_engine            rndEngine(benchmark.active ? 0 : (unsigned) time(nullptr));
		std::uniform_real_distribution<float> rndDist(-1.0f, 1.0f);
 
		// Initial particle positions
		std::vector<Particle> particleBuffer(PARTICLE_COUNT);
		for (auto &particle : particleBuffer)
		{
			particle.pos = glm::vec2(rndDist(rndEngine), rndDist(rndEngine));
			particle.color = glm::vec3(rndDist(rndEngine), rndDist(rndEngine), rndDist(rndEngine));
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
	
	void createUniformBufObj()
	{
		VkDeviceSize uniformsBufferSize = sizeof(uniforms);

		vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniBuf,
			uniformsBufferSize);

		uniBuf.map();

		updateUniformBufObj();
	}

	void updateUniformBufObj()
	{
		// Update time values we hold on CPU side
		uniforms.timeVals.x += frameTimer;
		uniforms.timeVals.y = uniforms.timeVals.x / 2;
		uniforms.timeVals.z = uniforms.timeVals.x * 2;
		uniforms.timeVals.w = uniforms.timeVals.x * 4;

		// Copy CPU values to GPU memory
		memcpy(uniBuf.mapped, &uniforms, sizeof(uniforms));
	}
	
	void createRenderPass()
	{
		VkAttachmentDescription colorAttach{};
		colorAttach.format = swapChain.colorFormat;
		colorAttach.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttach.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttach.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttach.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttach.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttach.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorAttachRef{};
		colorAttachRef.attachment = 0;
		colorAttachRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpassInfo{};
		subpassInfo.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassInfo.colorAttachmentCount = 1;
		subpassInfo.pColorAttachments = &colorAttachRef;

		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		auto renderPassInfo = vks::initializers::renderPassCreateInfo();
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &colorAttach;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpassInfo;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &graphics.renderPass));
	}
	
	void createDescriptorPool()
	{
		//TODO: Create descriptor pool and use member that is defined in example base
	}
	void createDescriptorSetLayout()
	{
		//TODO: Create descriptor set layout, one uniform buffer for anim purposes
	}
	void createDescriptorSet()
	{
		//TODO: Fill the descriptor set with vkWriteDescriptorSet
	}

	void createGraphicsPipeline()
	{
		//-----------------------------------
		auto vertShaderModule = vks::tools::loadShader(
			(getShadersPath() + "myComputeParticles/myparticles.vert.spv").c_str(),
			device);
		auto fragShaderModule = vks::tools::loadShader(
			(getShadersPath() + "myComputeParticles/myparticles.frag.spv").c_str(),
			device);

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
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipeLayoutInfo, nullptr, &graphics.pipeLayout));
		
		auto pipeInfo = vks::initializers::pipelineCreateInfo(graphics.pipeLayout, graphics.renderPass);
		// Shader stages
		pipeInfo.stageCount = 2;
		pipeInfo.pStages = shaderStages;

		// Fixed-functions stages
		pipeInfo.pVertexInputState = &vertexInputInfo;
		pipeInfo.pInputAssemblyState = &inputAssemblyInfo;
		pipeInfo.pViewportState = &viewportInfo;
		pipeInfo.pRasterizationState = &rasterInfo;
		pipeInfo.pMultisampleState = &multisampleInfo;
		pipeInfo.pDepthStencilState = nullptr;
		pipeInfo.pColorBlendState = &colorBlendInfo;
		pipeInfo.pDynamicState = nullptr; // Could be set to the one we created?

		pipeInfo.subpass = 0;
		
		VK_CHECK_RESULT( vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &graphics.pipe));
		
		//-----------------------------------
		vkDestroyShaderModule(device, fragShaderModule, nullptr);
		vkDestroyShaderModule(device, vertShaderModule, nullptr);
	}
	
	void recordCommandBuffers()
	{
		for(size_t i = 0; i < drawCmdBuffers.size(); ++i)
		{
			auto cmdBufBeginInfo = vks::initializers::commandBufferBeginInfo();
			cmdBufBeginInfo.flags = 0;
			cmdBufBeginInfo.pInheritanceInfo = nullptr;

			VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufBeginInfo));

			auto renderPassInfo = vks::initializers::renderPassBeginInfo();
			renderPassInfo.renderPass = graphics.renderPass;
			renderPassInfo.framebuffer = frameBuffers[i];
			renderPassInfo.renderArea = vks::initializers::rect2D(width, height, 0, 0);
			VkClearValue clearColor = {{{ 0, 0, 0, 1 }}};
			renderPassInfo.clearValueCount = 1;
			renderPassInfo.pClearValues = &clearColor;
			
			VkDeviceSize offsets[] = { 0 };
			
			vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics.pipe);
			vkCmdBindVertexBuffers(drawCmdBuffers[i], 0, 1, &storageBuffer.buffer, offsets);
			vkCmdDraw(drawCmdBuffers[i], PARTICLE_COUNT, 1, 0, 0);
			drawUI(drawCmdBuffers[i]);
			vkCmdEndRenderPass(drawCmdBuffers[i]);

			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
		}
	}
	
	void prepare() override
	{
		VulkanExampleBase::prepare();
		
		createStorageBuffer();
		createUniformBufObj();
		createRenderPass();
		createDescriptorPool();
		createDescriptorSetLayout();
		createDescriptorSet();
		createGraphicsPipeline();
		recordCommandBuffers();
		
		prepared = true;
	}

	void render() override
	{
		if (!prepared) return;
		
		draw();
		updateUniformBufObj();
	}

	void draw()
	{
		VulkanExampleBase::prepareFrame();

		VkSemaphore waitSemaphores[] = { semaphores.presentComplete };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];

		VkSemaphore signalSemaphores[] = { semaphores.renderComplete };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
		
		VulkanExampleBase::submitFrame();
	}

	void OnUpdateUIOverlay(vks::UIOverlay *overlay) override
	{
		if (overlay->header("Settings"))
		{
			overlay->checkBox("Attach?", &uiSettings.attach);
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

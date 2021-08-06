#include "vulkanexamplebase.h"

#define VERTEX_BUFFER_BIND_ID 0
#define ENABLE_VALIDATION false
#define PARTICLE_COUNT 200

class VulkanExample : public VulkanExampleBase
{
public:
	bool attach = false;
	
	struct
	{
		VkPipelineVertexInputStateCreateInfo inputState;
		std::vector<VkVertexInputBindingDescription> bindingDescs;
		std::vector<VkVertexInputAttributeDescription> attributeDescs;
	} vertexBinds;

	struct
	{
		uint32_t queueFamilyIndex;
		VkDescriptorSetLayout setLayout;
		VkDescriptorSet set;
		VkPipelineLayout pipelineLayout;
		VkPipeline pipeline;
		VkSemaphore semaphore;
	} graphics;
	
	// Resources for the compute part of the example
	struct
	{
		uint32_t              queueFamilyIndex;    // Used to check if compute and graphics queue families differ and require additional barriers
		vks::Buffer           storageBuffer;       // (Shader) storage buffer object containing the particles
		vks::Buffer           uniformBuffer;       // Uniform buffer object containing particle system parameters
		VkQueue               queue;               // Separate queue for compute commands (queue family may differ from the one used for graphics)
		VkCommandPool         commandPool;         // Use a separate command pool (queue family may differ from the one used for graphics)
		VkCommandBuffer       commandBuffer;       // Command buffer storing the dispatch commands and barriers
		VkSemaphore           semaphore;           // Execution dependency between compute & graphic submission
		VkDescriptorSetLayout setLayout; // Compute shader binding layout
		VkDescriptorSet       set;       // Compute shader bindings
		VkPipelineLayout      pipelineLayout;      // Layout of the compute pipeline
		VkPipeline            pipeline;            // Compute pipeline for updating particle positions
		struct computeUBO
		{
			// Compute shader uniform block object
			float   deltaT; //		Frame delta time
			float   destX;  //		x position of the attractor
			float   destY;  //		y position of the attractor
			int32_t particleCount = PARTICLE_COUNT;
		}           ubo;
	}               compute;

	struct Particle
	{
		glm::vec2 pos;
		glm::vec2 vel;
		glm::vec3 color;
	};

	VulkanExample() : VulkanExampleBase(ENABLE_VALIDATION)
	{
		vertexBinds = {};
		graphics = {};

		title = "Compute Shader Sync Test";
		settings.overlay = true;
	}
	~VulkanExample() override
	{ }

	void buildCommandBuffers() override
	{
		auto commandBufInfo = vks::initializers::commandBufferBeginInfo();

		VkClearValue clearVals[2];
		clearVals[0].color = defaultClearColor;
		clearVals[1].depthStencil = { 1.f, 0 };

		auto renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.renderArea.offset.x = 0;
		renderPassBeginInfo.renderArea.offset.y = 0;
		renderPassBeginInfo.renderArea.extent.width = width;
		renderPassBeginInfo.renderArea.extent.height = height;
		renderPassBeginInfo.clearValueCount = 2;
		renderPassBeginInfo.pClearValues = clearVals;

		for(uint32_t i = 0; i <drawCmdBuffers.size(); ++i)
		{
			renderPassBeginInfo.framebuffer = frameBuffers[i];
			VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &commandBufInfo));

			//TODO: Should get barrier HERE

			vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			auto viewport = vks::initializers::viewport((float)width, (float)height, 0.f, 1.f);
			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

			auto scissors = vks::initializers::rect2D(width, height, 0, 0);
			vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissors);

			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics.pipeline);
			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics.pipelineLayout, 0, 1, &graphics.set, 0, nullptr);

			glm::vec2 screenDim = glm::vec2((float)width, (float)height);
			vkCmdPushConstants(
				drawCmdBuffers[i],
				graphics.pipelineLayout,
				VK_SHADER_STAGE_VERTEX_BIT,
				0,
				sizeof(glm::vec2),
				&screenDim);

			VkDeviceSize offsets[1] = {0};
			vkCmdBindVertexBuffers(drawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &compute.storageBuffer.buffer, offsets);
			vkCmdDraw(drawCmdBuffers[i], PARTICLE_COUNT, 1, 0, 0);

			drawUI(drawCmdBuffers[i]);
			vkCmdEndRenderPass(drawCmdBuffers[i]);

			//TODO: Should release barrier HERE

			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
		}
	}

	void buildComputeCommandBuffer()
	{
		//TODO: we will handle compute side of process later
	}

	void prepareStorageBuffers()
	{
		std::default_random_engine            rndEngine(benchmark.active ? 0 : (unsigned) time(nullptr));
		std::uniform_real_distribution<float> rndDist(-1.0f, 1.0f);
 
		// Initial particle positions
		std::vector<Particle> particleBuffer(PARTICLE_COUNT);
		for (auto &particle : particleBuffer)
		{
			particle.pos = glm::vec2(rndDist(rndEngine), rndDist(rndEngine));
			particle.vel = glm::vec2(0.0f);
			particle.color = glm::vec3(particle.pos, 1.f);
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
			&compute.storageBuffer,
			storageBufferSize);
 
		// Copy from staging buffer to storage buffer
		VkCommandBuffer copyCmd    = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		VkBufferCopy    copyRegion = {};
		copyRegion.size            = storageBufferSize;
		vkCmdCopyBuffer(copyCmd, stagingBuffer.buffer, compute.storageBuffer.buffer, 1, &copyRegion);
		// Execute a transfer barrier to the compute queue, if necessary
		//if (graphics.queueFamilyIndex != compute.queueFamilyIndex)
		//{
		//	VkBufferMemoryBarrier buffer_barrier =
		//	{
		//		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
		//		nullptr,
		//		VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
		//		0,
		//		graphics.queueFamilyIndex,
		//		compute.queueFamilyIndex,
		//		compute.storageBuffer.buffer,
		//		0,
		//		compute.storageBuffer.size
		//	};
 
		//	vkCmdPipelineBarrier(
		//		copyCmd,
		//		VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
		//		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		//		0,
		//		0, nullptr,
		//		1, &buffer_barrier,
		//		0, nullptr);
		//}
		vulkanDevice->flushCommandBuffer(copyCmd, queue, true);
 
		stagingBuffer.destroy();
 
		// Binding description
		vertexBinds.bindingDescs.resize(1);
		vertexBinds.bindingDescs[0] =
			vks::initializers::vertexInputBindingDescription(
				VERTEX_BUFFER_BIND_ID,
				sizeof(Particle),
				VK_VERTEX_INPUT_RATE_VERTEX);
 
		// Attribute descriptions
		// Describes memory layout and shader positions
		vertexBinds.attributeDescs.resize(2);
		// Location 0 : Position
		vertexBinds.attributeDescs[0] =
			vks::initializers::vertexInputAttributeDescription(
				VERTEX_BUFFER_BIND_ID,
				0,
				VK_FORMAT_R32G32_SFLOAT,
				offsetof(Particle, pos));
		// Location 1 : color
		vertexBinds.attributeDescs[1] =
			vks::initializers::vertexInputAttributeDescription(
				VERTEX_BUFFER_BIND_ID,
				1,
				VK_FORMAT_R32G32B32_SFLOAT,
				offsetof(Particle, color));
 
		// Assign to vertex buffer
		vertexBinds.inputState = vks::initializers::pipelineVertexInputStateCreateInfo();
		vertexBinds.inputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexBinds.bindingDescs.size());
		vertexBinds.inputState.pVertexBindingDescriptions = vertexBinds.bindingDescs.data();
		vertexBinds.inputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexBinds.attributeDescs.size());
		vertexBinds.inputState.pVertexAttributeDescriptions = vertexBinds.attributeDescs.data();
	}

	void setupDescriptorPool()
	{
		std::vector<VkDescriptorPoolSize> poolSizes =
		{
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2)
		};
 
		VkDescriptorPoolCreateInfo descriptorPoolInfo =
			vks::initializers::descriptorPoolCreateInfo(
				static_cast<uint32_t>(poolSizes.size()),
				poolSizes.data(),
				2);
 
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));
	}
	
	void setupDescriptorSetLayout()
	{
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings;

		auto descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(
			setLayoutBindings.data(),
			static_cast<uint32_t>(setLayoutBindings.size()));

		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(
			device,
			&descriptorLayout,
			nullptr,
			&graphics.setLayout));

		auto pipelineLayoutInfo = vks::initializers::pipelineLayoutCreateInfo(
			&graphics.setLayout,
			1);

		auto pushConstRange = vks::initializers::pushConstantRange(
			VK_SHADER_STAGE_VERTEX_BIT,
			sizeof(glm::vec2),
			0);

		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &pushConstRange;

		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &graphics.pipelineLayout));
	}

	void setupDescriptorSet()
	{
		VkDescriptorSetAllocateInfo allocInfo =
			vks::initializers::descriptorSetAllocateInfo(
				descriptorPool,
				&graphics.setLayout,
				1);

		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &graphics.set));

		// Maybe we'll want to add some sampler later...
		//std::vector<VkWriteDescriptorSet> writeDescriptorSets;
		//// Binding 0 : Particle color map
		//writeDescriptorSets.push_back(vks::initializers::writeDescriptorSet(
		//	graphics.descriptorSet,
		//	VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		//	0,
		//	&textures.particle.descriptor));
		//// Binding 1 : Particle gradient ramp
		//writeDescriptorSets.push_back(vks::initializers::writeDescriptorSet(
		//	graphics.descriptorSet,
		//	VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		//	1,
		//	&textures.gradient.descriptor));

		//vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
	}

	void preparePipelines()
	{
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
			vks::initializers::pipelineInputAssemblyStateCreateInfo(
				VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
				0,
				VK_FALSE);

		VkPipelineRasterizationStateCreateInfo rasterizationState =
			vks::initializers::pipelineRasterizationStateCreateInfo(
				VK_POLYGON_MODE_FILL,
				VK_CULL_MODE_NONE,
				VK_FRONT_FACE_COUNTER_CLOCKWISE,
				0);

		VkPipelineColorBlendAttachmentState blendAttachmentState =
			vks::initializers::pipelineColorBlendAttachmentState(
				0xf,
				VK_FALSE);

		VkPipelineColorBlendStateCreateInfo colorBlendState =
			vks::initializers::pipelineColorBlendStateCreateInfo(
				1,
				&blendAttachmentState);

		VkPipelineDepthStencilStateCreateInfo depthStencilState =
			vks::initializers::pipelineDepthStencilStateCreateInfo(
				VK_FALSE,
				VK_FALSE,
				VK_COMPARE_OP_ALWAYS);

		VkPipelineViewportStateCreateInfo viewportState =
			vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);

		VkPipelineMultisampleStateCreateInfo multisampleState =
			vks::initializers::pipelineMultisampleStateCreateInfo(
				VK_SAMPLE_COUNT_1_BIT,
				0);

		std::vector<VkDynamicState> dynamicStateEnables = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};
		VkPipelineDynamicStateCreateInfo dynamicState =
			vks::initializers::pipelineDynamicStateCreateInfo(
				dynamicStateEnables.data(),
				static_cast<uint32_t>(dynamicStateEnables.size()),
				0);

		// Rendering pipeline
		// Load shaders
		std::array<VkPipelineShaderStageCreateInfo,2> shaderStages;

		shaderStages[0] = loadShader(getShadersPath() + "myComputeParticles/myparticles.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "myComputeParticles/myparticles.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

		VkGraphicsPipelineCreateInfo pipelineCreateInfo =
			vks::initializers::pipelineCreateInfo(
				graphics.pipelineLayout,
				renderPass,
				0);

		pipelineCreateInfo.pVertexInputState = &vertexBinds.inputState;
		pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
		pipelineCreateInfo.pRasterizationState = &rasterizationState;
		pipelineCreateInfo.pColorBlendState = &colorBlendState;
		pipelineCreateInfo.pMultisampleState = &multisampleState;
		pipelineCreateInfo.pViewportState = &viewportState;
		pipelineCreateInfo.pDepthStencilState = &depthStencilState;
		pipelineCreateInfo.pDynamicState = &dynamicState;
		pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCreateInfo.pStages = shaderStages.data();
		pipelineCreateInfo.renderPass = renderPass;

		// Additive blending
		blendAttachmentState.colorWriteMask = 0xF;
		blendAttachmentState.blendEnable = VK_TRUE;
		blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
		blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
		blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
		blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;

		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &graphics.pipeline));
	}

	void prepareGraphics()
	{
		prepareStorageBuffers();
		setupDescriptorSetLayout();
		preparePipelines();
		setupDescriptorSet();

		auto semaphoreInfo = vks::initializers::semaphoreCreateInfo();
		VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &graphics.semaphore));
	}

	void draw()
	{
		VulkanExampleBase::prepareFrame();

		VkPipelineStageFlags graphicsWaitStageMask[] = {
			 VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
			 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT 
		};
		
		VkSemaphore graphicsWaitSemaphores[] = { semaphores.presentComplete };
		VkSemaphore graphicsSignalSemaphores[] = {
			graphics.semaphore,
			semaphores.renderComplete
		};

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = graphicsWaitSemaphores;
		submitInfo.pWaitDstStageMask = graphicsWaitStageMask;
		submitInfo.signalSemaphoreCount = 2;
		submitInfo.pSignalSemaphores = graphicsSignalSemaphores;
		VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));

		VulkanExampleBase::submitFrame();
	}

	void prepare()
	{
		VulkanExampleBase::prepare();
		
		graphics.queueFamilyIndex = vulkanDevice->queueFamilyIndices.graphics;
		
		setupDescriptorPool();
		prepareGraphics();
		buildCommandBuffers();
		
		prepared = true;
	}
	
	void render() override
	{
		if (!prepared) return;
		draw();
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

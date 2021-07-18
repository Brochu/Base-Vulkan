#include "vulkanexamplebase.h"

#define ENABLE_VALIDATION false

class VulkanExample : public VulkanExampleBase
{
public:
	VulkanExample() : VulkanExampleBase(ENABLE_VALIDATION)
	{
		title = "My version of compute shader particles";
		settings.overlay = true;
	}
	
	~VulkanExample() override
	{
	}

	void render() override
	{
		std::cout << "[Render] Are we reaching render yet?\n";
	}
	
	void OnUpdateUIOverlay(vks::UIOverlay *overlay) override
	{
		if (overlay->header("Settings"))
		{
			overlay->text("testing ...");
		}
	}
	
};

VULKAN_EXAMPLE_MAIN()

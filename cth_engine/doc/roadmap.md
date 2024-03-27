# roadmap

## 1. modernize everything
### in progress:
	HlcShader.hpp/cpp

### completed (partially untested):
	vulkan:
		base:
		- CthDevice.hpp/cpp
		- CthInstance.hpp/cpp

		memory:
			buffer:
			- CthBuffer.hpp/cpp
			- CthDefaultBuffer.hpp/cpp
			descriptor:
			- CthDescriptedResource.hpp/cpp
			- CthDescriptor.hpp/cpp
			- CthDescriptorSet.hpp/cpp
			- CthDescriptorPool.hpp/cpp

		surface:
		- CthSwapchain.hpp/cpp
		- CthWindow.hpp/cpp


		pipeline:
		- CthPipeline.hpp/cpp
			layout:
			- CthGraphicsPipelineConfig.hpp/cpp
			- CthPipelineLayout.hpp/cpp
			- CthDescriptorSetLayout.hpp/cpp
			shader:
			- CthShader.hpp/cpp

		render:
		- CthRenderer.hpp/cpp (partially needs to be extended with a render pass abstraction)
		
		debug:
		- CthDebugMessenger.hpp/cpp


## 3. modernize further
	- new memory system and model management
	- add render pass wrapper
		- add wrapper ptr to GraphicsPipelineConfig
	- maybe add a command buffer manager
	- put all the constructors at the top fuck me

## 4. implement dear imgui support
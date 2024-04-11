# roadmap

## 1. modernize everything
### in progress:
	
### completed (partially):
	interface:
		model:
		- CthVertex.hpp/cpp

	vulkan:
		base:
		- CthDevice.hpp/cpp
		- CthInstance.hpp/cpp
		
		debug:
		- CthDebugMessenger.hpp/cpp
		
		pipeline:
		- CthPipeline.hpp/cpp
			layout:
			- CthPipelineLayout.hpp/cpp
			- CthDescriptorSetLayout.hpp/cpp
			shader:
			- CthShader.hpp/cpp
		
		render:
		- CthRenderer.hpp/cpp 
		
		resource:
			buffer:
			- CthBuffer.hpp/inl
			- CthDefaultBuffer.hpp/cpp
			descriptor:
			- CthDescriptedResource.hpp/cpp
			- CthDescriptor.hpp/cpp
			- CthDescriptorSet.hpp/cpp
			- CthDescriptorPool.hpp/cpp
				descriptors:
				- CthBufferDescriptors.hpp
				- CthImageDescriptors.hpp
			image:
			- CthBasicImage.hpp/cpp
			- CthImage.hpp/cpp
			- CthImageView.hpp/cpp
				texture
				- CthTexture.hpp/cpp
				- CthSampler.hpp/cpp

		surface:
		- CthSwapchain.hpp/cpp
		- CthWindow.hpp/cpp
		- CthSurface.hpp/cpp





		


## 3. todo
	- render pass abstraction
	- command buffer abstraction
	- command pool abstraction

	- framebuffer abstraction?

## 4. implement dear imgui support
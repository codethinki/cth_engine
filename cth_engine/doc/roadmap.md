# roadmap

## 1. modernize everything



## 3. todo
	- basic variant of pipeline
	- basic variant of shader
	- use mem::const_ptr everywhere especially with the device passing around
	- create basic physical device and basic logical device
	- use VkDevice handle in the static void destroy of every class_
	- it should be surface->window not window->surface
	- think about move semantics for every vulkan object (maybe enable them)
	- render pass abstraction
	- Queue abstraction
	- Move functions that are not about presenting from the swapchain to the renderer
	- One Time submit command buffer abstraction


	- framebuffer abstraction?

## 4. implement dear imgui support
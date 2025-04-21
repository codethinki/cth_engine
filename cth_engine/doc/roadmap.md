# roadmap

## 3. todo
	- replace CONSTANT_DEBUG_MODE with CTH_DEBUG_MODE
	- use CTH_ASSERT for DEBUG_CHECK... instead of CTH_ERROR
	- modernize pipeline
	- modernize shader
	- expand use of vk::not_null and cth::not_null
	- use VkDevice handle in the static void destroy of every class
	- it should be surface->window not window->surface
	- convert CthDestructionQueue mechanism to lambdas which catch their dependencies handles by value (use std::function) 
	- think about move semantics for every vulkan object (maybe enable them)
	- Move functions that are not about presenting from the swapchain to the renderer
	- move all debug checks to the hpp file and remove the preprocessor macros

## 4. implement dear imgui support

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




//continue in basic swapchain.cpp


//sync pattern:

- present submit to present queue with renderFinished semaphore for n - 1
- wait for n - 1 acquire fence
- start acquire with semaphore and fence for n
- start render stuff for n and record the command buffers
- submit to graphics queue with the acquire semaphore for n as top of pipe wait stage and register another renderFinished for n
- present submit to present queue with the render finished for n
- wait for the acquire fence for n
...
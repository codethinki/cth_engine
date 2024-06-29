#pragma once
#include <vector>


namespace cth {
class BasicSemaphore;


struct GraphicsSyncConfig {
    /**
     * expects semaphores[currentFrame] to be signaled after rendering
     * presents the image once the semaphore is signaled
     */
    std::vector<const BasicSemaphore*> renderFinishedSemaphores;

    /**
     * semaphores[currentFrame] will be signaled once the image is free to render on
     * expects that the semaphore will be waited before rendering
     */
    std::vector<const BasicSemaphore*> imageAvailableSemaphores;
};
} //namespace cth

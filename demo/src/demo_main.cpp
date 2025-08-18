#include "HlcApp.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "jolly/utility/CthOSWindow.hpp"

#include <stb_image.h>

#include "vk/utility/cth_vk_format.hpp"

#include "vk/base/jvk.hpp"


using namespace std;
using namespace cth;

int main() {
    cth::log::msg<except::INFO>("exec dir: {}", std::filesystem::current_path().string());

    vk::Jvk::init();
    vk::OSWindow::init();


    unique_ptr<App> app = make_unique<App>();
    app->run();
    app = nullptr;
    //try {

    /* }
     catch(const cth::except::result_exception& e) {
         cth::out::error.println(e.string());
         std::terminate();
     }
     catch(const cth::except::default_exception& e) {
         cth::out::error.println(e.string());
         std::terminate();
     }
     catch(const std::exception& e) {
         cth::out::error.println(e.what());
         std::terminate();
     }
     catch() {
         cth::out::error.println("Unknown exception");
         std::terminate();
     }*/

    vk::Jvk::terminate();
    return EXIT_SUCCESS;
}

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define VOLK_IMPLEMENTATION
#include <volk.h>

import cth.io.log;

import cth.engine;

import demo.app;

using namespace std;
using namespace cth;

int main() {
    cth::log::msg<except::INFO>("exec dir: {}", std::filesystem::current_path().string());

    vk::EngineControl::init();


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

    vk::EngineControl::terminate();
    return EXIT_SUCCESS;
}

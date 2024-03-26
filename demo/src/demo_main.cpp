#include "HlcApp.hpp"

#include <cth/cth_log.hpp>


#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <fstream>


using namespace std;
using namespace cth;
#ifndef _FINAL
int main() {
    cth::log::msg<except::INFO>("exec dir: {}", cth::filesystem::current_path().string());
#else
#include <Windows.h>
INT WINAPI WinMain(HINSTANCE h_instance, HINSTANCE h_prev_instance, char* lp_cmd_line, int n_cmd_show) {
    const unique_ptr<ofstream> logStream = make_unique<ofstream>("log.txt", ios::ate);
    cth::log::setLogStream(out::col_stream{logStream.get()});
#endif

    Engine::init();


    unique_ptr<App> app = make_unique<App>();
    app->run();
    app = nullptr;
    //try {

    /* }
     catch(const cth::except::vk_result_exception& e) {
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
     catch(...) {
         cth::out::error.println("Unknown exception");
         std::terminate();
     }*/

    Engine::terminate();
    return EXIT_SUCCESS;
}

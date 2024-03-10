#include <cstdlib>
#include <future>
#include <iostream>
#include <random>

#include <filesystem>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <__msvc_filebuf.hpp>

#include "HlcApp.hpp"
#include "vulkan/utils/cth_vk_specific_utils.hpp"


#ifdef _DEBUG
int main() {
#else
#include <Windows.h>
int main() { //TEMP edit this back to be invisible 
//INT WINAPI WinMain(HINSTANCE h_instance, HINSTANCE h_prev_instance, char* lp_cmd_line, int n_cmd_show) {
#endif
    using namespace std;
    using namespace cth;


    App app{};
    try { app.run(); }
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
    }
    return EXIT_SUCCESS;
}

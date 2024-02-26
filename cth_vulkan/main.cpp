#include <cstdlib>
#include <future>
#include <iostream>
#include <random>

#include <filesystem>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <__msvc_filebuf.hpp>

#include "HlcApp.hpp"

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
    catch(const exception& e) {
        cerr << e.what() << endl;
#ifdef _DEBUG
        throw runtime_error("stop");
        return EXIT_FAILURE;
#else
		other::messageBox(L"Program Error", L"Program crashed :(", MB_OK | MB_ICONERROR);
		return EXIT_FAILURE;
#endif
    }
    return EXIT_SUCCESS;
}

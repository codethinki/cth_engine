#pragma once
#include "CthVertex.hpp"

#include <string>


namespace cth {
using namespace std;

class ModelManager;

struct FrameInfo;

class Model {
public:
    /**
     * @param manager ptr to ModelManager
     * @param name model name (unique)
     * @param id managers model id
     * @param filepath path to model.obj
     * @param mtl_base_dir path to material dir, empty -> same dir
     */
    explicit Model(ModelManager* manager, const string& name, uint32_t id, const string& filepath, const string& mtl_base_dir = "");



    void render(uint32_t first_instance, uint32_t instances, const FrameInfo& frame_info); //IMPLEMENT render function

    const string name; /*model name (unique)*/
    const uint32_t id; /*model id in manager list*/
    const string path; /*path to model.obj*/
    const string mtlBaseDir; /*path to material dir, empty -> same dir*/
    bool loaded = false;

private:
    void load();
    void unload();

    vector<Vertex> vertices{};
    vector<uint32_t> indices{};

    ModelManager* manager;

    friend ModelManager;
};


}

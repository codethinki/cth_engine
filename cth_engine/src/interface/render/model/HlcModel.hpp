#pragma once
#include "CthVertex.hpp"

#include <string>


namespace cth::vk {

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
    explicit Model(ModelManager* manager, std::string const& name, uint32_t id, std::string const& filepath, std::string const& mtl_base_dir = "");



    void render(uint32_t first_instance, uint32_t instances, FrameInfo const& frame_info); //IMPLEMENT render function

    std::string const name; /*model name (unique)*/
    uint32_t const id; /*model id in manager list*/
    std::string const path; /*path to model.obj*/
    std::string const mtlBaseDir; /*path to material dir, empty -> same dir*/
    bool loaded = false;

private:
    void load();
    void unload();

    std::vector<Vertex> vertices{};
    std::vector<uint32_t> indices{};

    ModelManager* manager;

    friend ModelManager;
};


}

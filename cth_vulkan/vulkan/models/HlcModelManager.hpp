#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <tiny_obj_loader.h>
#include<glm/glm.hpp>

namespace cth {
using namespace std;

class RenderObject;

class Model;

class ModelManager {
	explicit ModelManager(const string& model_list_dir = MODEL_LIST_DIR);

public:
	
	/**
	 * \brief calls loadModel for unloaded models with obj.getReqModels value
	 * \param objects object receives ptrs to requested models
	 */
	void loadModels(const vector<unique_ptr<RenderObject>>& objects);
	/**
	 * \brief calls loadModel for unloaded models
	 * \return vec of model ptrs
	 */
	vector<Model*> loadModels(const vector<string>& model_names);

	/**
	 * \brief loads model data from models obj file
	 * \param model_name unique name of model
	 */
	void loadModel(const string& model_name);

	/**
	 * \brief registers models from list as available
	 * \param model_list_path path to model list
	 */
	void loadModelList(const string& model_list_path);

	[[nodiscard]] glm::vec2 getMaterialUV(const string& material_name); //IMPLEMENT getMaterialUv
	[[nodiscard]] glm::vec2 registerMaterial(const tinyobj::material_t& material); //IMPLEMENT registerMaterial


	[[nodiscard]] string getMtlBaseDir(const string& model_name);
	[[nodiscard]] string getMtlBaseDir(uint32_t model_id);

	static constexpr uint32_t MAX_MODELS = 100;
private:
	vector<Model> models = [] {
		vector<Model> vec{};
		vec.reserve(MAX_MODELS);
		return vec;
	}();
	unordered_map<string, uint32_t> modelIds = []{
		unordered_map<string, uint32_t> map{};
		map.reserve(MAX_MODELS);
		return map;
	}();

	unordered_map<string, uint32_t> materialIds{};
	vector<tinyobj::material_t> uniqueMaterials{};
	vector<glm::vec2> materialUVs{};

	static constexpr char MODEL_LIST_DIR[] = "resources/models/model_list.txt";
};


}
#include "HlcModelManager.hpp"

//#include "HlcModel.hpp"
//
//#include "../objects/HlcRenderObject.hpp"
//
//
//
//namespace cth {
//
//ModelManager::ModelManager(const string& model_list_dir) {
//	loadModelList(model_list_dir);
//}
//
//void ModelManager::loadModels(const vector<unique_ptr<RenderObject>>& objects) {
//	for(auto& object : objects) {
//		object->recModelPtrs(
//			loadModels(object->getReqModels())
//			);
//	}
//}
//vector<Model*> ModelManager::loadModels(const vector<string>& model_names) {
//	vector<Model*> loadedModels{};
//	loadedModels.reserve(model_names.size());
//
//	for(const auto& modelName : model_names) {
//		assert(modelIds.contains(modelName) && "loadModels: unknown model requested");
//		const uint32_t modelId = modelIds[modelName];
//
//		if(!loadedModels[modelId]) loadModel(modelName);
//
//		loadedModels.push_back(&models[modelId]);
//	}
//	return loadedModels;
//}
//
//void ModelManager::loadModel(const string& model_name) {
//	const uint32_t modelId = modelIds[model_name];
//	assert(!models[modelId].loaded && "load: model already loaded");
//
//	models[modelId].load();
//}
//
//void ModelManager::loadModelList(const string& model_list_path) {
//	ifstream file(model_list_path);
//	assert(file.is_open() && "loadModelList: model list not found");
//
//	//set mtl dir
//	string line;
//	getline(file, line);
//	string mtlBaseDir = line;
//
//	//get model data
//	while(getline(file, line)) {
//		istringstream iss(line);
//		string name, path;
//		getline(iss, name, ',');
//		getline(iss, path, ';');
//
//		assert(!modelIds.contains(name) && "loadModelList: model registered twice");
//
//		modelIds[name] = models.size();
//		models.emplace_back(this, name, models.size(), path, mtlBaseDir);
//
//		assert(MAX_MODELS <= models.size() && "loadModelList: max model count reached");
//	}
//}
//
//string ModelManager::getMtlBaseDir(const string& model_name) {
//	assert(modelIds.contains(model_name) && "getMtlBaseDir: model must be in loaded model list");
//	return models[modelIds[model_name]].mtlBaseDir;
//}
//string ModelManager::getMtlBaseDir(const uint32_t model_id) { return models[model_id].mtlBaseDir; }
//
//
//}
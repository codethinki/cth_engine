#include "HlcModel.hpp"

#include "HlcModelManager.hpp"


#include <stdexcept>
//#include <tiny_obj_loader.h>


//namespace cth {

//Model::Model(ModelManager* manager, const string& name, const uint32_t id, const string& filepath, const string& mtl_base_dir) : name(name), id(id),
//	path(filepath), mtlBaseDir(mtl_base_dir), manager(manager) { assert(!filepath.empty() && "empty path provided"); }
//
//
//void Model::load() {
//	tinyobj::attrib_t attrib;
//	vector<tinyobj::shape_t> shapes;
//	vector<tinyobj::material_t> materials;
//	string warn, err;
//
//	if(!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str()), mtlBaseDir, true, true)
//		throw std::runtime_error(
//			"load: failed to load model: " + name + "\nat: " + path + " \n" + warn + err);
//
//
//	for(const auto& shape : shapes) {
//		if(shape.mesh.num_face_vertices.size() != shape.mesh.indices.size() / 3) throw runtime_error("load: faces have to be triangles");
//		//TEMP for testing
//
//		int indexOffset = 0;
//
//		unordered_map<Vertex, uint32_t> uniqueVertices{};
//
//		for(int faceID = 0; faceID < shape.mesh.num_face_vertices.size(); faceID++) {
//			int materialID = shape.mesh.material_ids[faceID];
//			//for every vertex in triangle
//			for(int v = 0; v < 3; v++) {
//				const auto& index = shape.mesh.indices[indexOffset + v];
//				Vertex vertex{};
//
//				if(index.vertex_index >= 0)
//					vertex.position = {
//						attrib.vertices[3 * index.vertex_index + 0],
//						attrib.vertices[3 * index.vertex_index + 1],
//						attrib.vertices[3 * index.vertex_index + 2]
//					};
//
//				if(index.normal_index >= 0)
//					vertex.normal = {
//						attrib.normals[3 * index.normal_index + 0],
//						attrib.normals[3 * index.normal_index + 1],
//						attrib.normals[3 * index.normal_index + 2]
//					};
//
//				if(index.texcoord_index >= 0)
//					vertex.uv = {
//						attrib.texcoords[2 * index.texcoord_index + 0],
//						attrib.texcoords[2 * index.texcoord_index + 1]
//					};
//
//				if(materials.size() > 0) {
//					glm::vec2 materialUV = manager->getMaterialUV(materials[materialID].name);
//
//					if(materialUV.x < 0 && materialUV.y < 0) materialUV = manager->registerMaterial(materials[materialID]);
//					vertex.materialUV = materialUV;
//				}
//				else vertex.materialUV = manager->getMaterialUV("default");
//
//				if(!uniqueVertices.contains(vertex)) {
//					uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
//					vertices.push_back(vertex);
//				}
//				indices.push_back(uniqueVertices[vertex]);
//			}
//
//			indexOffset += 3;
//		}
//	}
//
//	loaded = true;
//
//	//IMPLEMENT let model manager allocate model
//}
//void Model::unload() {
//	vertices.clear();
//	indices.clear();
//
//	loaded = false;
//	//IMPLEMENT let model manager deallocate
//}
//
//
//}

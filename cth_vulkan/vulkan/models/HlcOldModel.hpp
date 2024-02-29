#pragma once
#include "HlcVertex.hpp"
#include "..\memory\CthBuffer.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <memory>
#include <unordered_map>
#include <vector>

namespace cth {
using namespace std;

class OldModel {
public:
	struct Builder {
		std::vector<Vertex> vertices{};
		std::vector<uint32_t> indices{};
		explicit Builder(const vector<Vertex>& obj_vertices);
		Builder(const vector<Vertex>& vertices, const vector<uint32_t>& indices) : vertices{vertices}, indices{indices} {}
	};

	void bind(VkCommandBuffer command_buffer) const;
	void draw(VkCommandBuffer command_buffer) const;

private:
	Device& hlcDevice;

	unique_ptr<Buffer> vertexBuffer;
	uint32_t vertexCount = 0;

	bool hasIndexBuffer = false;
	unique_ptr<Buffer> indexBuffer;
	uint32_t indexCount{};

	void createVertexBuffers(const std::vector<Vertex>& vertices);
	void createIndexBuffers(const std::vector<uint32_t>& indices);

	inline static unordered_map<string, unique_ptr<OldModel>> models{};
public:
	static std::unique_ptr<OldModel> createSquareModel(Device& device, glm::vec3 position, glm::vec2 extent);
	static void regModel(const string& model_name, unique_ptr<OldModel> model);
	static OldModel* model(const string& model_name);
	static void delModel(const string& model_name);
	static void clearModels() { models.clear(); };

	OldModel(Device& device, const Builder& builder);
	~OldModel();
	OldModel(const OldModel&) = delete;
	OldModel& operator=(const OldModel&) = delete;
};

}

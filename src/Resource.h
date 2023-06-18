#pragma once

#include <optional>
#include <stack>
#include <map>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "util/NamespaceDecl.h"
#include "util/File.h"
#include "core/HostImage.h"
#include "Model.h"

class Resource {
public:
	friend class Scene;

	const std::vector<MeshVertex>& vertices() const { return mVertices; }
	const std::vector<uint32_t>& indices() const { return mIndices; }
	const std::vector<MeshInstance>& meshInstances() const { return mMeshInstances; }
	const std::vector<ModelInstance*>& modelInstances() const { return mModelInstances; }
	const std::vector<Material>& materials() const { return mMaterials; }
	const std::vector<int32_t>& materialIndices() const { return mMaterialIndices; }

	zvk::HostImage* getImageByIndex(uint32_t index);
	zvk::HostImage* getImageByPath(const File::path& path);
	std::optional<uint32_t> addImage(const File::path& path, zvk::HostImageType type);
	std::vector<zvk::HostImage*>& imagePool() { return mImagePool; }

	ModelInstance* openModelInstance(
		const File::path& path,
		glm::vec3 pos, glm::vec3 scale = glm::vec3(1.0f), glm::vec3 rotation = glm::vec3(0.0f));

	void clearDeviceMeshAndImage();
	void destroy();

private:
	ModelInstance* getModelInstanceByPath(const File::path& path);
	ModelInstance* createNewModelInstance(const File::path& path);
	MeshInstance createNewMeshInstance(aiMesh* mesh, const aiScene* scene);

private:
	std::vector<MeshVertex> mVertices;
	std::vector<uint32_t> mIndices;
	std::vector<MeshInstance> mMeshInstances;
	std::vector<ModelInstance*> mModelInstances;
	std::vector<Material> mMaterials;
	std::vector<int32_t> mMaterialIndices;

	std::vector<zvk::HostImage*> mImagePool;
	std::map<File::path, uint32_t> mMapPathToImageIndex;
	std::map<File::path, ModelInstance*> mMapPathToModelInstance;
};
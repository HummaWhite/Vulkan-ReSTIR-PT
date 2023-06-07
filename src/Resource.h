#pragma once

#include <optional>
#include <stack>
#include <map>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "util/NamespaceDecl.h"
#include "util/File.h"
#include "HostImage.h"
#include "Model.h"

class Resource {
public:
	friend class Scene;

	zvk::HostImage* getImageByIndex(uint32_t index);
	zvk::HostImage* getImageByPath(const File::path& path);
	std::optional<uint32_t> addImage(const File::path& path, zvk::HostImageType type);
	std::vector<zvk::HostImage*>& imagePool() { return mImagePool; }

	ModelInstance* createNewModelInstance(const File::path& path);
	ModelInstance* getModelInstanceByPath(const File::path& path);
	ModelInstance* openModelInstance(
		const File::path& path,
		glm::vec3 pos, glm::vec3 scale = glm::vec3(1.0f), glm::vec3 rotation = glm::vec3(0.0f));

	void clearDeviceMeshAndImage();
	void destroy();

private:
	MeshInstance createNewMeshInstance(aiMesh* mesh, const aiScene* scene);

private:
	std::vector<MeshVertex> mVertices;
	std::vector<uint32_t> mIndices;
	std::vector<ModelInstance*> mModelInstances;
	std::vector<Material> mMaterials;

	std::vector<zvk::HostImage*> mImagePool;
	std::map<File::path, uint32_t> mMapPathToImageIndex;
	std::map<File::path, ModelInstance*> mMapPathToModelInstance;
};
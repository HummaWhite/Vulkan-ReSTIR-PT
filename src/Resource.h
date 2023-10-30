#pragma once

#include <optional>
#include <stack>
#include <map>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "util/NamespaceDecl.h"
#include "util/File.h"
#include "util/AliasTable.h"
#include "core/HostImage.h"
#include "Model.h"

class Resource {
public:
	enum MeshType {
		Object = 0,
		Light,
		MeshTypeCount
	};

public:
	friend class Scene;
	Resource();
	~Resource() { destroy(); }

	float getModelTransformedSurfaceArea(const ModelInstance* modelInstance, bool isLight) const;

	zvk::HostImage* getImageByIndex(uint32_t index);
	zvk::HostImage* getImageByPath(const File::path& path);
	std::optional<uint32_t> addImage(const File::path& path, zvk::HostImageType type);
	std::vector<zvk::HostImage*>& imagePool() { return mImagePool; }
	const std::vector<zvk::HostImage*>& imagePool() const { return mImagePool; }

	ModelInstance* openModelInstance(
		const File::path& path, bool isLight,
		glm::vec3 pos, glm::vec3 scale = glm::vec3(1.0f), glm::vec3 rotation = glm::vec3(0.0f));

	void clearDeviceMeshAndImage();
	void destroy();

private:
	ModelInstance* getModelInstanceByPath(const File::path& path);
	ModelInstance* createNewModelInstance(const File::path& path, bool isLight);
	MeshInstance createNewMeshInstance(aiMesh* mesh, const aiScene* scene, bool isLight);

public:
	std::vector<MeshVertex> vertices[MeshTypeCount];
	std::vector<uint32_t> indices[MeshTypeCount];
	std::vector<MeshInstance> meshInstances[MeshTypeCount];
	std::vector<ModelInstance*> modelInstances[MeshTypeCount];
	std::vector<ModelInstance*> uniqueModelInstances[MeshTypeCount];
	std::vector<Material> materials;
	std::vector<int32_t> materialIndices;

private:
	std::vector<zvk::HostImage*> mImagePool;
	std::map<File::path, uint32_t> mMapPathToImageIndex;
	std::map<File::path, ModelInstance*> mMapPathToUniqueModelInstance;
};
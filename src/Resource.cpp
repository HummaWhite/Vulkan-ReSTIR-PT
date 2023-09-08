#include "Resource.h"
#include "util/Error.h"

#include <thread>
#include <atomic>

zvk::HostImage* Resource::getImageByIndex(uint32_t index) {
	Log::check(index < mImagePool.size(), "Image index out of bound");
	return mImagePool[index];
}

zvk::HostImage* Resource::getImageByPath(const File::path& path) {
	auto res = mMapPathToImageIndex.find(path);
	if (res == mMapPathToImageIndex.end()) {
		return nullptr;
	}
	return getImageByIndex(res->second);
}

std::optional<uint32_t> Resource::addImage(const File::path& path, zvk::HostImageType type) {
	auto res = mMapPathToImageIndex.find(path);

	if (res != mMapPathToImageIndex.end()) {
		return res->second;
	}
	auto img = zvk::HostImage::createFromFile(path, type, 4);

	if (!img) {
		return std::nullopt;
	}
	mMapPathToImageIndex[path] = mImagePool.size();
	mImagePool.push_back(img);
	return mImagePool.size() - 1;
}

std::vector<ObjectInstance> Resource::objectInstances() const {
	std::vector<ObjectInstance> instances;

	for (auto instance : mModelInstances) {
		glm::mat4 transform = instance->modelMatrix();
		glm::mat4 transformInv = glm::inverse(transform);
		glm::mat4 transformInvT = glm::transpose(transformInv);

		instances.push_back({
			transform, transformInv, transformInvT,
			mMeshInstances[instance->meshOffset()].indexOffset,
			instance->mNumIndices,
			getModelTransformedSurfaceArea(instance),
			static_cast<float>(rand()) / RAND_MAX
		});
	}
	return instances;
}

float Resource::getModelTransformedSurfaceArea(const ModelInstance* modelInstance) const {
	float area = 0.f;

	const auto& beginMeshInstance = mMeshInstances[modelInstance->meshOffset()];
	uint32_t indexOffset = beginMeshInstance.indexOffset;
	uint32_t indexCount = modelInstance->numIndices();

	auto transform = [&](const glm::vec3& pos) {
		return glm::vec3(modelInstance->modelMatrix() * glm::vec4(pos, 1.f));
	};

	for (uint32_t i = 0; i < indexCount / 3; i++) {
		glm::vec3 va = transform(mVertices[mIndices[indexOffset + i * 3 + 0]].pos);
		glm::vec3 vb = transform(mVertices[mIndices[indexOffset + i * 3 + 1]].pos);
		glm::vec3 vc = transform(mVertices[mIndices[indexOffset + i * 3 + 2]].pos);
		area += .5f * glm::length(glm::cross(vb - va, vc - va));
	}
	return area;
}

ModelInstance* Resource::openModelInstance(const File::path& path, glm::vec3 pos, glm::vec3 scale, glm::vec3 rotation) {
	auto model = getModelInstanceByPath(path);

	if (model == nullptr) {
		model = createNewModelInstance(path);
		model->mRefId = mUniqueModelInstances.size();
		mMapPathToModelInstance[path] = model;
		mUniqueModelInstances.push_back(model);
	}
	auto newCopy = model->copy();
	newCopy->setPos(pos);
	newCopy->setScale(scale);
	newCopy->setRotation(rotation);

	mModelInstances.push_back(newCopy);
	return newCopy;
}

ModelInstance* Resource::createNewModelInstance(const File::path& path) {
	auto model = new ModelInstance;
	auto pathStr = path.generic_string();

	model->mPath = pathStr;
	Assimp::Importer importer;
	uint32_t option = aiProcess_Triangulate
		| aiProcess_FlipUVs
		| aiProcess_GenSmoothNormals
		| aiProcess_GenUVCoords
		| aiProcess_FixInfacingNormals
		| aiProcess_FindInstances
		| aiProcess_JoinIdenticalVertices
		| aiProcess_OptimizeMeshes
		| aiProcess_GenUVCoords
		//| aiProcess_ForceGenNormals
		//| aiProcess_FindDegenerates
		;
	auto scene = importer.ReadFile(pathStr.c_str(), option);

	Log::line<1>("ModelInstance loading: " + pathStr + " ...");
	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
		Log::line<1>("Assimp " + std::string(importer.GetErrorString()));
		return nullptr;
	}

	std::stack<aiNode*> stack;
	stack.push(scene->mRootNode);
	model->mMeshOffset = mMeshInstances.size();

	while (!stack.empty()) {
		auto node = stack.top();
		stack.pop();

		for (int i = 0; i < node->mNumMeshes; i++) {
			auto mesh = scene->mMeshes[node->mMeshes[i]];
			auto meshInstance = createNewMeshInstance(mesh, scene);
			model->mNumIndices += meshInstance.indexCount;
			model->mNumVertices += meshInstance.vertexCount;
			mMeshInstances.push_back(meshInstance);
		}
		for (int i = 0; i < node->mNumChildren; i++) {
			stack.push(node->mChildren[i]);
		}
		model->mNumMeshes += node->mNumMeshes;
	}

	for (int i = 0; i < scene->mNumMaterials; i++) {
		aiMaterial* aiMat = scene->mMaterials[i];
		aiColor3D albedo;
		aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, albedo);

		Material material;
		material.baseColor.r = albedo.r;
		material.baseColor.g = albedo.g;
		material.baseColor.b = albedo.b;

		if (aiMat->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
			aiString str;
			aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &str);
			File::path imagePath(str.C_Str());

			if (!path.is_absolute()) {
				imagePath = path.parent_path() / imagePath;
			}
			Log::line<2>("Albedo texture " + imagePath.generic_string());

			auto imageIdx = addImage(imagePath, zvk::HostImageType::Int8);
			material.textureIdx = imageIdx ? *imageIdx : InvalidResourceIdx;
		}
		else {
			material.textureIdx = InvalidResourceIdx;
		}
		mMaterials.push_back(material);
	}
	Log::line<2>(std::to_string(scene->mNumMaterials) + " material(s)");
	Log::line<2>(std::to_string(model->numMeshes()) + " mesh(es)");
	return model;
}

ModelInstance* Resource::getModelInstanceByPath(const File::path& path) {
	auto res = mMapPathToModelInstance.find(path);
	if (res == mMapPathToModelInstance.end()) {
		return nullptr;
	}
	return res->second;
}

void Resource::clearDeviceMeshAndImage() {
	for (auto image : mImagePool) {
		delete image;
	}

	for (auto m : mMapPathToModelInstance) {
		delete m.second;
	}
	mVertices.clear();
	mIndices.clear();
	mImagePool.clear();
	mMapPathToImageIndex.clear();
	mMapPathToModelInstance.clear();
}

void Resource::destroy() {
	clearDeviceMeshAndImage();
	mModelInstances.clear();
	mMaterials.clear();
}

MeshInstance Resource::createNewMeshInstance(aiMesh* mesh, const aiScene* scene) {
	Log::line<2>("Mesh nVertices = " + std::to_string(mesh->mNumVertices) +
		", nFaces = " + std::to_string(mesh->mNumFaces));

	MeshInstance meshInstance;
	uint32_t vertexOffset = mVertices.size();

	for (int i = 0; i < mesh->mNumVertices; i++) {
		MeshVertex vertex;
		vertex.pos = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
		vertex.norm = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };
		vertex.uvx = mesh->mTextureCoords[0] ? mesh->mTextureCoords[0][i].x : 0;
		vertex.uvy = mesh->mTextureCoords[0] ? mesh->mTextureCoords[0][i].y : 0;
		mVertices.push_back(vertex);
	}
	uint32_t materialOffset = mMaterials.size();

	if (scene->mNumMaterials > 0 && mesh->mMaterialIndex >= 0) {
		meshInstance.materialIdx = mesh->mMaterialIndex + materialOffset;
	}
	meshInstance.vertexOffset = vertexOffset;
	meshInstance.vertexCount = mesh->mNumVertices;
	meshInstance.indexOffset = mIndices.size();

	for (int i = 0; i < mesh->mNumFaces; i++) {
		aiFace face = mesh->mFaces[i];
		for (int j = 0; j < face.mNumIndices; j++) {
			mIndices.push_back(face.mIndices[j] + vertexOffset);
		}
		meshInstance.indexCount += face.mNumIndices;
		mMaterialIndices.push_back(meshInstance.materialIdx);
	}
	return meshInstance;
}

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

std::optional<uint32_t> Resource::addImage(const File::path& path, zvk::HostImageType type, zvk::HostImageFilter filter) {
	auto res = mMapPathToImageIndex.find(path);

	if (res != mMapPathToImageIndex.end()) {
		return res->second;
	}
	auto img = zvk::HostImage::createFromFile(path, type, filter, 4);

	if (!img) {
		return std::nullopt;
	}
	mMapPathToImageIndex[path] = static_cast<uint32_t>(mImagePool.size());
	mImagePool.push_back(img);
	return mImagePool.size() - 1;
}

Resource::Resource() {
	Material emptyMat;
	emptyMat.baseColor = glm::vec3(1.f, 0.f, 1.f);
	materials.push_back(emptyMat);
}

float Resource::getModelTransformedSurfaceArea(const ModelInstance* modelInstance, bool isLight) const {
	std::atomic<float> area = 0.f;

	const auto& beginMeshInstance = meshInstances[isLight][modelInstance->meshOffset()];
	uint32_t indexOffset = beginMeshInstance.indexOffset;
	uint32_t indexCount = modelInstance->numIndices();
	uint32_t triangleCount = indexCount / 3;

	auto transform = [&](const glm::vec3& pos) {
		return glm::vec3(modelInstance->modelMatrix() * glm::vec4(pos, 1.f));
	};

	auto areaReduction = [&](uint32_t begin, uint32_t end) {
		for (uint32_t i = begin; i < end; i++) {
			uint32_t ia = indices[isLight][indexOffset + i * 3 + 0];
			uint32_t ib = indices[isLight][indexOffset + i * 3 + 1];
			uint32_t ic = indices[isLight][indexOffset + i * 3 + 2];

			glm::vec3 va = transform(vertices[isLight][ia].pos);
			glm::vec3 vb = transform(vertices[isLight][ib].pos);
			glm::vec3 vc = transform(vertices[isLight][ic].pos);
			area += .5f * glm::length(glm::cross(vb - va, vc - va));
		}
	};

	uint32_t maxThreads = std::min(triangleCount, std::thread::hardware_concurrency());
	std::vector<std::thread> threads(maxThreads);

	for (uint32_t i = 0; i < maxThreads; i++) {
		uint32_t begin = i * (triangleCount / maxThreads);
		uint32_t end = std::min((i + 1) * (triangleCount / maxThreads), triangleCount);
		threads[i] = std::thread(areaReduction, begin, end);
	}

	for (auto& thread : threads) {
		thread.join();
	}
	return area;
}

ModelInstance* Resource::openModelInstance(const File::path& path, bool isLight, glm::vec3 pos, glm::vec3 scale, glm::vec3 rotation) {
	auto model = getModelInstanceByPath(path, isLight);

	if (model == nullptr) {
		model = createNewModelInstance(path, isLight);
		model->mRefId = static_cast<uint32_t>(uniqueModelInstances[isLight].size());
		mMapPathToUniqueModelInstance[isLight][path] = model;
		uniqueModelInstances[isLight].push_back(model);
	}
	auto newCopy = model->copy();
	newCopy->setPos(pos);
	newCopy->setScale(scale);
	newCopy->setRotation(rotation);

	modelInstances[isLight].push_back(newCopy);
	return newCopy;
}

ModelInstance* Resource::createNewModelInstance(const File::path& path, bool isLight) {
	auto model = new ModelInstance;
	auto pathStr = path.generic_string();

	model->mPath = pathStr;
	Assimp::Importer importer;

	uint32_t option = 0
		| aiProcess_Triangulate
		| aiProcess_FlipUVs
		| aiProcess_GenUVCoords
		| aiProcess_FixInfacingNormals
		//| aiProcess_FindInstances
		//| aiProcess_JoinIdenticalVertices
		//| aiProcess_OptimizeMeshes
		//| aiProcess_ForceGenNormals
		//| aiProcess_FindDegenerates
		;
	option |= isLight ? aiProcess_GenNormals : aiProcess_GenSmoothNormals;

	auto scene = importer.ReadFile(pathStr.c_str(), option);

	Log::line<1>("ModelInstance loading: " + pathStr + " ...");
	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
		Log::line<1>("Assimp " + std::string(importer.GetErrorString()));
		return nullptr;
	}

	std::stack<aiNode*> stack;
	stack.push(scene->mRootNode);
	model->mMeshOffset = static_cast<uint32_t>(meshInstances[isLight].size());

	while (!stack.empty()) {
		auto node = stack.top();
		stack.pop();

		for (uint32_t i = 0; i < node->mNumMeshes; i++) {
			auto mesh = scene->mMeshes[node->mMeshes[i]];
			auto meshInstance = createNewMeshInstance(mesh, scene, isLight);
			model->mNumIndices += meshInstance.indexCount;
			model->mNumVertices += meshInstance.vertexCount;
			meshInstances[isLight].push_back(meshInstance);
		}
		for (uint32_t i = 0; i < node->mNumChildren; i++) {
			stack.push(node->mChildren[i]);
		}
		model->mNumMeshes += node->mNumMeshes;
	}

	if (!isLight) {
		for (uint32_t i = 0; i < scene->mNumMaterials; i++) {
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

				auto imageIdx = addImage(imagePath, zvk::HostImageType::Int8, zvk::HostImageFilter::Linear);
				material.textureIdx = imageIdx ? *imageIdx : InvalidResourceIdx;
			}
			else {
				material.textureIdx = InvalidResourceIdx;
			}
			materials.push_back(material);
		}
	}
	Log::line<2>(std::to_string(scene->mNumMaterials) + " material(s)");
	Log::line<2>(std::to_string(model->numMeshes()) + " mesh(es)");
	return model;
}

ModelInstance* Resource::getModelInstanceByPath(const File::path& path, bool isLight) {
	return nullptr;
	auto res = mMapPathToUniqueModelInstance[isLight].find(path);
	if (res == mMapPathToUniqueModelInstance[isLight].end()) {
		return nullptr;
	}
	return res->second;
}

void Resource::clearDeviceMeshAndImage() {
	for (auto image : mImagePool) {
		delete image;
	}

	for (auto m : mMapPathToUniqueModelInstance[MeshType::Object]) {
		delete m.second;
	}
	for (auto m : mMapPathToUniqueModelInstance[MeshType::Light]) {
		delete m.second;
	}

	for (uint32_t i = 0; i < MeshTypeCount; i++) {
		vertices[i].clear();
		indices[i].clear();
	}
	mImagePool.clear();
	mMapPathToImageIndex.clear();
	mMapPathToUniqueModelInstance[MeshType::Object].clear();
	mMapPathToUniqueModelInstance[MeshType::Light].clear();
}

void Resource::destroy() {
	clearDeviceMeshAndImage();
	materials.clear();

	for (uint32_t i = 0; i < MeshTypeCount; i++) {
		modelInstances[i].clear();
	}
}

MeshInstance Resource::createNewMeshInstance(aiMesh* mesh, const aiScene* scene, bool isLight) {
	Log::line<2>("Mesh nVertices = " + std::to_string(mesh->mNumVertices) +
		", nFaces = " + std::to_string(mesh->mNumFaces));

	MeshInstance meshInstance;
	auto vertexOffset = static_cast<uint32_t>(vertices[isLight].size());

	for (uint32_t i = 0; i < mesh->mNumVertices; i++) {
		MeshVertex vertex;
		vertex.pos = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
		vertex.norm = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };
		vertex.uvx = mesh->mTextureCoords[0] ? mesh->mTextureCoords[0][i].x : 0;
		vertex.uvy = mesh->mTextureCoords[0] ? mesh->mTextureCoords[0][i].y : 0;
		vertices[isLight].push_back(vertex);
	}
	auto materialOffset = static_cast<uint32_t>(materials.size());

	if (scene->mNumMaterials > 0 && mesh->mMaterialIndex >= 0) {
		meshInstance.materialIdx = mesh->mMaterialIndex + materialOffset;
	}
	meshInstance.vertexOffset = vertexOffset;
	meshInstance.vertexCount = mesh->mNumVertices;
	meshInstance.indexOffset = static_cast<uint32_t>(indices[isLight].size());

	for (uint32_t i = 0; i < mesh->mNumFaces; i++) {
		aiFace face = mesh->mFaces[i];
		for (uint32_t j = 0; j < face.mNumIndices; j++) {
			indices[isLight].push_back(face.mIndices[j] + vertexOffset);
		}
		meshInstance.indexCount += face.mNumIndices;

		if (!isLight) {
			materialIndices.push_back(meshInstance.materialIdx);
		}
	}
	return meshInstance;
}

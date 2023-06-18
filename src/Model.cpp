#include "Model.h"

const glm::mat4 ConstRot = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

void ModelInstance::rotateLocal(float angle, glm::vec3 axis) {
	mRotMatrix = glm::rotate(mRotMatrix, glm::radians(angle), axis);
}

void ModelInstance::rotateWorld(float angle, glm::vec3 axis) {
	mRotMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(angle), axis) * mRotMatrix;
}

glm::mat4 ModelInstance::modelMatrix() const {
	glm::mat4 model(1.0f);
	model = glm::translate(model, mPos);
	model = glm::scale(model, mScale);
	//model = model * ConstRot;
	model = glm::rotate(model, glm::radians(mRotation.x), glm::vec3(0.0f, 0.0f, 1.0f));
	model = glm::rotate(model, glm::radians(mRotation.y), glm::vec3(1.0f, 0.0f, 0.0f));
	model = glm::rotate(model, glm::radians(mRotation.z), glm::vec3(0.0f, 1.0f, 0.0f));
	return model;
}

ModelInstance* ModelInstance::copy() const {
	auto model = new ModelInstance;
	*model = *this;
	model->mName = mName + "\'";
	return model;
}

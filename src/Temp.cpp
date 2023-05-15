#include <vulkan/vulkan.hpp>

void temp() {
    vk::AccelerationStructureGeometryKHR geom;
    geom.setGeometryType(vk::GeometryTypeKHR::eAabbs);
    vk::AccelerationStructureGeometryDataKHR data;
    vk::AccelerationStructureGeometryAabbsDataKHR aabb;
    data.setAabbs(aabb);
    VkAccelerationStructureGeometryDataKHR d;
    geom.setGeometry(data);
}
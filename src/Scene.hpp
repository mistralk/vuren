#ifndef SCENE_HPP
#define SCENE_HPP

#include "Common.hpp"
#include "Camera.hpp"

#include <string>
#include <vector>

namespace vuren {

class Scene {
public:
    Scene() {}
    ~Scene() {}

    SceneObject getObject(uint32_t objectId) { return m_objects[objectId]; }

    const std::vector<SceneObject> &getObjects() { return m_objects; }

    const std::vector<SceneObjectDevice> &getObjectsDevice() { return m_objectsDevice; }

    const std::vector<ObjectInstance> &getInstances() { return m_instances; }

    const std::vector<std::shared_ptr<Texture>> &getTextures() { return m_textures; }

    const std::vector<Material> &getMaterials() { return m_materials; }

    void addObject(SceneObject object) { m_objects.emplace_back(object); }

    void addObjectDevice(SceneObjectDevice object) { m_objectsDevice.emplace_back(object); }

    void addInstances(const std::vector<ObjectInstance> &instances) {
        m_instances.insert(m_instances.end(), instances.begin(), instances.end());
    }

    void addTexture(std::shared_ptr<Texture> pTexture) { m_textures.emplace_back(pTexture); }

    void addMaterial(Material material) { m_materials.emplace_back(material); }

    void setInstanceCount(uint32_t objectId, uint32_t count) { m_objects[objectId].instanceCount = count; }

    Camera& getCamera() {
        return m_camera;
    }

private:
    std::vector<SceneObject> m_objects;
    std::vector<SceneObjectDevice> m_objectsDevice;

    std::vector<ObjectInstance> m_instances;
    std::vector<std::shared_ptr<Texture>> m_textures;
    std::vector<Material> m_materials;

    SceneGlobalData m_globalData;
    Camera m_camera;
};

} // namespace vuren

#endif // SCENE_HPP
#ifndef SCENE_HPP
#define SCENE_HPP

#include "Common.hpp"

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

    const std::vector<Texture> &getTextures() { return m_textures; }

    void addObject(SceneObject object) { m_objects.emplace_back(object); }

    void addObjectDevice(SceneObjectDevice object) { m_objectsDevice.emplace_back(object); }

    void addInstances(const std::vector<ObjectInstance> &instances) {
        m_instances.insert(m_instances.end(), instances.begin(), instances.end());
    }

    void addTexture(Texture texture) { m_textures.emplace_back(texture); }

    void setInstanceCount(uint32_t objectId, uint32_t count) { m_objects[objectId].instanceCount = count; }

private:
    std::vector<SceneObject> m_objects;
    std::vector<SceneObjectDevice> m_objectsDevice;

    std::vector<ObjectInstance> m_instances;
    std::vector<Texture> m_textures;
};

} // namespace vuren

#endif // SCENE_HPP
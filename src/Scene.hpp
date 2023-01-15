#ifndef SCENE_HPP
#define SCENE_HPP

#include "Common.hpp"

#include <string>
#include <vector>

namespace vrb {

class Scene {
public:
    Scene() {}
    ~Scene() {}

    SceneObject getObject(uint32_t id) {
        return m_objects[id];
    }

    const std::vector<SceneObject>& getObjects() {
        return m_objects;
    }

    const std::vector<SceneObjectDevice>& getObjectsDevice() {
        return m_objectsDevice;
    }

    const std::vector<ObjectInstance>& getInstances() {
        return m_instances;
    }

    const std::vector<Texture>& getTextures() {
        return m_textures;
    }

    void addObject(SceneObject object) {
        m_objects.emplace_back(object);
    }

    void addObjectDevice(SceneObjectDevice object) {
        m_objectsDevice.emplace_back(object);
    }

    void addInstance(ObjectInstance instance) {
        m_instances.emplace_back(instance);
    }

    void addTexture(Texture texture) {
        m_textures.emplace_back(texture);
    }

private:
    std::vector<SceneObject> m_objects;
    std::vector<SceneObjectDevice> m_objectsDevice;

    std::vector<ObjectInstance> m_instances;
    std::vector<Texture> m_textures;

};

} // namespace vrb

#endif // SCENE_HPP
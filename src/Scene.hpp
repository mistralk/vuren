#ifndef SCENE_HPP
#define SCENE_HPP

#include "Common.hpp"
#include "ResourceManager.hpp"

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

    uint32_t getInstanceCount() {
        return m_instanceCount;
    }

    void addObject(SceneObject object) {
        m_objects.push_back(object);
    }

    void setInstanceCount(uint32_t instanceCount) {
        m_instanceCount = instanceCount;
    }

private:
    std::vector<SceneObject> m_objects;

    uint32_t m_instanceCount;

};

} // namespace vrb

#endif // SCENE_HPP
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

    void addObject(SceneObject object) {
        m_objects.push_back(object);
    }

private:
    std::vector<SceneObject> m_objects;

};

} // namespace vrb

#endif // SCENE_HPP
#ifndef CAMERA_HPP
#define CAMERA_HPP

#include "Common.hpp"
#include <chrono>

namespace vuren {

class Camera {
public:
    Camera() {}
    ~Camera() {}

    const CameraData &getData() { return m_data; }

    void init();

    void forward(const float speed);
    void dolly(const float speed);
    void rotate(float oldX, float oldY);

    bool isLeftMousePressed() { return m_pressedLeftMouse; }
    void pressLeftMouse(float oldX, float oldY);
    void releaseLeftMouse(float oldX, float oldY);

    void setMappedCameraBuffer(void *mappedBuffer) { m_pMappedBuffer = mappedBuffer; }

    void setExtent(vk::Extent2D extent) { m_extent = extent; }

    void updateCamera();

    void exampleRotationalCamera();

private:
    glm::vec3 m_eye, m_center, m_up;
    CameraData m_data;
    void *m_pMappedBuffer;
    vk::Extent2D m_extent;

    bool m_pressedLeftMouse{ false };
    float m_oldMouseX;
    float m_oldMouseY;
    bool m_bRotate{ false };
    bool m_bPan{ false };
    bool m_bForward{ false };


}; // class Camera

} // namespace vuren

#endif // CAMERA_HPP
#include "Camera.hpp"

namespace vuren {

void Camera::init() {
    m_eye    = glm::vec3(0.0f, 2.0f, 2.0f);
    m_center = glm::vec3(0.0f, 0.0f, 0.0f);
    m_up     = glm::vec3(0.0f, 1.0f, 0.0f);
    updateCamera();
}

// to-fix: accurate camera manipulation

void Camera::forward(const float speed) {
    glm::vec3 dir = glm::normalize(glm::vec3(m_center - m_eye));

    m_eye += dir * speed;
    m_center += dir * speed;
}

void Camera::dolly(const float speed) {
    glm::vec3 dir   = glm::normalize(glm::vec3(m_center - m_eye));
    glm::vec3 right = glm::cross(dir, m_up);

    m_eye += right * speed;
    m_center += right * speed;
}

void Camera::rotate(float newX, float newY) {
    float dx        = m_oldMouseX - newX;
    float dy        = m_oldMouseY - newY;
    glm::vec3 dir   = glm::normalize(glm::vec3(m_center - m_eye));
    glm::vec3 right = glm::cross(dir, m_up);

    glm::vec4 dir4d  = glm::vec4(dir, 1.0f);
    glm::vec4 newDir = dir4d * glm::rotate(glm::mat4(1.0f), 0.0004f * glm::radians(dx), m_up) *
                       glm::rotate(glm::mat4(1.0f), 0.0004f * glm::radians(dy), right);
    m_center = m_eye + glm::vec3(newDir);
}

void Camera::pressLeftMouse(float oldX, float oldY) {
    m_pressedLeftMouse = true;
    m_oldMouseX        = oldX;
    m_oldMouseY        = oldY;
}

void Camera::releaseLeftMouse(float oldX, float oldY) {
    m_pressedLeftMouse = false;
    m_oldMouseX        = oldX;
    m_oldMouseY        = oldY;
}

void Camera::updateCamera() {
    m_data.view = glm::lookAt(m_eye, m_center, m_up);
    m_data.proj = glm::perspective(glm::radians(45.0f), m_extent.width / (float) m_extent.height, 0.1f, 100.0f);
    m_data.proj[1][1] *= -1;

    // precompute inverse matrices for camera ray generation
    m_data.invView = glm::inverse(m_data.view);
    m_data.invProj = glm::inverse(m_data.proj);

    memcpy(m_pMappedBuffer, &m_data, sizeof(m_data));
}

void Camera::exampleRotationalCamera() {
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time       = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    m_eye    = glm::vec3(3.0 * glm::cos(time * glm::radians(90.0f)), 3.0 * glm::sin(time * glm::radians(90.0f)), 2.0f);
    m_center = glm::vec3(0.0f, 0.0f, 0.0f);
    m_up     = glm::vec3(0.0f, 0.0f, 1.0f);

    m_data.view = glm::lookAt(m_eye, m_center, m_up);
    m_data.proj = glm::perspective(glm::radians(45.0f), m_extent.width / (float) m_extent.height, 0.1f, 100.0f);

    // GLM's Y coordinate of the clip coordinates is inverted
    // To compensate this, flip the sign on the scaling factor of the Y axis in the proj matrix.
    m_data.proj[1][1] *= -1;

    // for camera ray generation
    m_data.invView = glm::inverse(m_data.view);
    m_data.invProj = glm::inverse(m_data.proj);

    // m_pScene->setCamera(camera);
    memcpy(m_pMappedBuffer, &m_data, sizeof(m_data));
}

} // namespace vuren
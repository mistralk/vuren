#include "Timer.hpp"

namespace vrb {

Timer::Timer() :
    m_start(std::chrono::steady_clock::now()) {
}

void Timer::reset() {
    m_start = std::chrono::steady_clock::now();
}

double Timer::elapsed() {
    auto currentTime = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<double, Milliseconds>(currentTime - m_start);
    m_start = currentTime;
    return elapsed.count();
}

} // namespace vrb
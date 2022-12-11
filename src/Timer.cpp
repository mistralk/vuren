#include "Timer.hpp"

namespace vrb {

Timer::Timer() :
    m_start(std::chrono::steady_clock::now()) {
}

void Timer::reset() {
    m_start = std::chrono::steady_clock::now();
}

double Timer::stop() {
    auto elapsed = std::chrono::duration<double, Milliseconds>(std::chrono::steady_clock::now() - m_start);
    return elapsed.count();
}

} // namespace vrb
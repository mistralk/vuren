#ifndef TIMER_HPP
#define TIMER_HPP

#include <chrono>

namespace vrb {

class Timer {
public:
    using Milliseconds = std::ratio<1, 1000>;

    Timer();
    ~Timer() = default;

    void reset();
    double stop();

private:
    std::chrono::steady_clock::time_point m_start;

};

} // namespace vrb

#endif // TIMER_HPP
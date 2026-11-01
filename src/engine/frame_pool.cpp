#include "engine/frame_pool.hpp"

namespace camera::engine {

FramePool::FramePool(std::size_t count) {
    frames_.reserve(count);
    for (std::size_t index = 0; index < count; ++index) frames_.push_back(std::make_shared<Frame>());
}

std::shared_ptr<FramePool::Frame> FramePool::acquire() {
    std::scoped_lock lock(mutex_);
    for (const auto& frame : frames_) {
        if (frame.use_count() == 1) return frame;
    }
    return {};
}

void FramePool::release(const std::shared_ptr<Frame>& frame) {
    (void)frame;
}

} // namespace camera::engine

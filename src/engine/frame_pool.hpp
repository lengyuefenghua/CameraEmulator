#pragma once

#include <cstddef>
#include <memory>
#include <mutex>
#include <vector>

namespace camera::engine {

class FramePool final {
public:
    struct Frame {
        mutable std::vector<std::byte> bytes;
    };
    using FrameRef = std::shared_ptr<const Frame>;

    explicit FramePool(std::size_t count = 3);
    [[nodiscard]] std::shared_ptr<Frame> acquire();
    void release(const std::shared_ptr<Frame>& frame);
    [[nodiscard]] std::size_t size() const noexcept { return frames_.size(); }

private:
    mutable std::mutex mutex_;
    std::vector<std::shared_ptr<Frame>> frames_;
};

} // namespace camera::engine

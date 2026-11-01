#include "engine/frame_pool.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>

TEST_CASE("Frame pool does not reuse a buffer while a send reference is held", "[FramePool]") {
    camera::engine::FramePool pool(1);
    auto frame = pool.acquire();
    REQUIRE(frame);
    frame->bytes = {std::byte{0x11}, std::byte{0x22}};
    camera::engine::FramePool::FrameRef inFlight = frame;
    frame.reset();
    REQUIRE_FALSE(pool.acquire());
    REQUIRE(inFlight->bytes.size() == 2);
    inFlight.reset();
    auto reusable = pool.acquire();
    REQUIRE(reusable);
}

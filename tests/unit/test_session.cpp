#include "core/camera_model.hpp"
#include "core/register_map.hpp"
#include "engine/session_controller.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Control privilege is exclusive and releases on zero", "[Session]") {
    camera::core::RegisterMap registers(camera::core::CameraConfig{});
    camera::engine::SessionController session(registers);
    std::string error;

    REQUIRE(session.acquire("127.0.0.10", 2, error));
    REQUIRE(session.owns("127.0.0.10"));
    REQUIRE_FALSE(session.acquire("127.0.0.11", 2, error));
    REQUIRE(session.release("127.0.0.10"));
    REQUIRE_FALSE(session.owns("127.0.0.10"));
    REQUIRE(session.acquire("127.0.0.11", 2, error));
    REQUIRE(session.write_register("127.0.0.11", 0x938, 500, error));
    REQUIRE(registers.read_u32(0x938) == 500);
}

#pragma once

#include "engine/engine_facade.hpp"

namespace camera::app {

class EngineClient final {
public:
    [[nodiscard]] engine::EngineFacade& backend() noexcept { return backend_; }
    [[nodiscard]] const engine::EngineFacade& backend() const noexcept { return backend_; }

private:
    engine::EngineFacade backend_;
};

} // namespace camera::app

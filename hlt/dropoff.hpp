#pragma once

#include "entity.hpp"

#include <memory>

namespace hlt {
    struct Dropoff : Entity {
        using Entity::Entity;

        bool is_fake = false;

        static std::shared_ptr<Dropoff> _generate(PlayerId player_id);
    };
}

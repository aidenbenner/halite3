#pragma once

#include <vector>
#include "direction.hpp"
#include "position.hpp"

namespace hlt {
    typedef int Halite;
    typedef int PlayerId;
    typedef int EntityId;

    typedef std::vector<std::vector<int>> VVI;
    typedef std::vector<std::vector<Position>> VVP;

    struct BFSR {
        VVI dist;
        VVP parent;
    };
}

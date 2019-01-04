#pragma once

#include <vector>
#include "direction.hpp"
#include "position.hpp"

#define VC std::vector
#define PB push_back

namespace hlt {
    typedef int Halite;
    typedef int PlayerId;
    typedef int EntityId;

    typedef std::vector<std::vector<int>> VVI;
    typedef std::vector<std::vector<int>> VVI;
    typedef std::vector<std::vector<Position>> VVP;


    struct TimePos {
        int turn;
        Position p;
        friend bool operator<(const TimePos& a, const TimePos& b) {
            if (a.turn ==  b.turn) {
                return a.p < b.p;
            }
            return a.turn < b.turn;
        }
    };

    enum ShipState {
        GATHERING,
        RETURNING,
        SUPER_RETURN,
        DROPOFF,
    };

    struct BFSR {
        VVI dist;
        VVP parent;
        VVI turns;
    };

    // COLLISION TERMS
    // avoid - never walk within range 1 of a ship that can walk into you
    // tolerate - never walk directly into
    // ignore - DGAF
    // SMART - only risk collision if you less halite
    enum EnemyResponse {
        AVOID,
        TOLERATE,
        SMART,
        IGNORE
    };

    struct RandomWalkResult {
        Direction bestdir;
        double cost;
        int turns;
        VC<Position> walk;
    };

    struct ShipPos {
        int id;
        Position pos;

        bool operator<(const ShipPos &b) const {
            if (id == b.id) {
                return pos < b.pos;
            }
            return id < b.id;
        }
    };




}

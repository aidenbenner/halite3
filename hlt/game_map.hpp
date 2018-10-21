#pragma once

#include "types.hpp"
#include "map_cell.hpp"
#include "player.hpp"

#include<cassert>
#include <vector>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <set>
#include <deque>
#include <string>

using namespace std;
namespace hlt {

struct pairhash {
public:
  template <typename T, typename U>
  std::size_t operator()(const std::pair<T, U> &x) const
  {
    return std::hash<T>()(x.first) + std::hash<U>()(x.second) * 255;
  }
};

    struct GameMap {
        int width;
        int height;
        std::vector<std::vector<MapCell>> cells;

        MapCell* at(const Position& position) {
            Position normalized = normalize(position);
            return &cells[normalized.y][normalized.x];
        }

        MapCell* at(int x, int y) {
            return at(Position{x,y});
        }

        MapCell* at(const Entity& entity) {
            return at(entity.position);
        }

        MapCell* at(const Entity* entity) {
            return at(entity->position);
        }

        MapCell* at(const std::shared_ptr<Entity>& entity) {
            return at(entity->position);
        }

        int calculate_distance(const Position& source, const Position& target) {
            const auto& normalized_source = normalize(source);
            const auto& normalized_target = normalize(target);

            const int dx = std::abs(normalized_source.x - normalized_target.x);
            const int dy = std::abs(normalized_source.y - normalized_target.y);

            const int toroidal_dx = std::min(dx, width - dx);
            const int toroidal_dy = std::min(dy, height - dy);

            return toroidal_dx + toroidal_dy;
        }

        Position normalize(const Position& position) {
            const int x = ((position.x % width) + width) % width;
            const int y = ((position.y % height) + height) % height;
            return { x, y };
        }


        bool canMove(std::shared_ptr<Ship> ship) {
            log::log(std::to_string(at(ship)->halite));
            log::log(std::to_string(ship->halite));
            return at(ship)->halite * 0.1 <= ship->halite;
        }

        std::vector<Direction> get_unsafe_moves(const Position& source, const Position& destination) {
            const auto& normalized_source = normalize(source);
            const auto& normalized_destination = normalize(destination);

            const int dx = std::abs(normalized_source.x - normalized_destination.x);
            const int dy = std::abs(normalized_source.y - normalized_destination.y);
            const int wrapped_dx = width - dx;
            const int wrapped_dy = height - dy;

            std::vector<Direction> possible_moves;

            if (normalized_source.x < normalized_destination.x) {
                possible_moves.push_back(dx > wrapped_dx ? Direction::WEST : Direction::EAST);
            } else if (normalized_source.x > normalized_destination.x) {
                possible_moves.push_back(dx < wrapped_dx ? Direction::WEST : Direction::EAST);
            }

            if (normalized_source.y < normalized_destination.y) {
                possible_moves.push_back(dy > wrapped_dy ? Direction::NORTH : Direction::SOUTH);
            } else if (normalized_source.y > normalized_destination.y) {
                possible_moves.push_back(dy < wrapped_dy ? Direction::NORTH : Direction::SOUTH);
            }

            if (dy > dx && possible_moves.size() >= 2) {
                auto tmp = possible_moves[0];
                possible_moves[0] = possible_moves[1];
                possible_moves[1] = tmp;
            }

            return possible_moves;
        }

        int naive_cost(int starting_halite, Position a, Position b) {
            if (a == b) return 0;
            int cost = at(a)->cost();
            int gain = at(a)->gain();
            if (cost > starting_halite) {
                cost -= gain + cost * 4 / 5;
            }
            return cost + naive_cost(starting_halite - cost, a.directional_offset(get_unsafe_moves(a,b)[0]), b);
        }

        int naive_waits(int starting_halite, Position a, Position b) {
            a = normalize(a);
            b = normalize(b);
            if (a == b) return 0;
            log::log(a);
            int cost = at(a)->cost();
            int gain = at(a)->gain();
            if (cost > starting_halite) {
                int curr_hal = starting_halite + gain; // - cost * 4 / 5;
                return 1 + naive_waits(curr_hal + gain,a, b);
            }
            if (at(a)->halite > 100) {
                int curr_hal = starting_halite + gain; // - cost * 4 / 5;
                return 1 + naive_waits(curr_hal + gain,a, b);
            }
            return 1 + naive_waits(starting_halite - cost, a.directional_offset(get_unsafe_moves(a,b)[0]), b);
        }

        Direction naive_navigate(std::shared_ptr<Ship> ship, const Position& destination) {
            // get_unsafe_moves normalizes for us
            ship->log("navigating");
            if (ship->position == destination)
                return Direction::STILL;
            if (true)
                mincostnav(ship->position, destination);
            for (auto direction : get_unsafe_moves(ship->position, destination)) {
                //Position target_pos = ship->position.directional_offset(direction);
                return direction;
                /*if (!at(target_pos)->is_unsafe()) {
                    at(target_pos)->mark_unsafe(ship);
                    return direction;
                }*/
            }
            return Direction::STILL;
        }

        unordered_map<pair<Position, Position>, pair<int, Direction>, pairhash> mincost;
        std::pair<int, Direction> mincostnav(const Position& start, const Position& dest) {
            mincost.clear();
            return mincostnav2(start,dest);
        }

        std::pair<int, Direction> fast_mincostnav(const Position& start, const Position& dest) {
            return mincostnav2(start,dest);
        }

        std::pair<int, Direction> clear_fast_mincostnav() {
            mincost.clear();
        }

        std::pair<int, Direction> mincostnav2(const Position& start, const Position& dest) {
            if (start == dest) {
                return make_pair(0, Direction::STILL);
            }
            auto p = make_pair(start, dest);
            if (mincost.count(p)) {
                return mincost[p];
            }

            auto moves = get_unsafe_moves(start, dest);
            int curr_weight = 666001;
            auto curr_move = Direction::STILL;

            int ccost = at(start)->cost();
            for (auto m : moves) {
                auto curr = mincostnav2(normalize(start.directional_offset(m)), dest);
                if (curr.first < curr_weight) {
                    curr_weight = curr.first;
                    curr_move = curr.second;
                }
            }
            //assert(curr_weight != 666001);
            if (curr_weight == 666001) curr_weight = 0;

           return mincost[p] = std::make_pair(ccost + curr_weight, curr_move);
        }

        const int TURN_WEIGHT = 30;

        int costfn(Ship *s, Position shipyard, Position dest) {
            if (dest == shipyard) return 100000;
            int cost = 0; // mincostnav(s->position, dest).first;

            int turns_to = calculate_distance(s->position, dest);
            int turns_back = calculate_distance(dest, shipyard);

            // TODO(@dropoff)
            int cost_back = 0; //mincostnav(dest, shipyard).first;
            int halite = at(dest)->halite;
            //if (halite < constants::MAX_HALITE / 10.0) return 8000 - halite + (turns_back + turns_to) * TURN_WEIGHT;

            int out = cost + turns_to * TURN_WEIGHT + turns_back * TURN_WEIGHT - halite * 100 + cost_back;
            out = max(0, halite - turns_to * TURN_WEIGHT - turns_back * TURN_WEIGHT);
            out = halite - cost - cost_back;
            out = halite - 100 * sqrt(turns_to + turns_back);
            return -out;
        }


        Position largestInArea(Position p, int r) {
            return largestInArea(p.x, p.y, r);
        }

        Position largestInArea(int x, int y, int r) {
            int curr_max = 0;
            int cx = 0;
            int cy = 0;
            for (int i = 0; i < 2 * r; i++) {
                for (int k = 0; k < 2 * r; k++) {
                    int h = at(x + i - r,y + k - r)->halite;
                    if (h > curr_max) {
                        curr_max = h;
                        cx = x + i - r;
                        cy = y + k - r;
                    }
                }
            }
            return Position {cx, cy};
        }

        void _update();
        static std::unique_ptr<GameMap> _generate();
    };
}

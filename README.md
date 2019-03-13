## Halite III
This is the code used to run the bot I wrote for the Halite III AI competition
ran by two sigma. I came 7th overall and was the highest ranked undergraduate
student. 

_Disclaimer:_ this is not clean code. A lot of things are hacky with 0 documentation because
I was the only one working on this project.

This was the first AI competition i've ever participated in and I had a lot of fun.
I spent about 4 months working on my bot.

Here is more information about the game https://halite.io/
Here is my player page https://halite.io/user/?user_id=1185

# Post-Mortem
Before reading you should read the overview of the rules for halite 3 from 
the page above. Halite 3 is a resource management game.

Here is a screen cap of my bot in action.
![Halite](https://i.imgur.com/TmiKKkw.png)
(One of my few wins against teccles, ReCurse and TheDuck all players with bots
that smash mine typically.) One thing you 
can even notice from a single screen cap of the game is that *it's really hard
to figure out what's going on*. I think this was an interesting challenge
in Halite. When playing against the top players there would be games you would just lose and have no idea why you lost them besides
the fact that they just 'mined better than you'. There was never any 'secret sauce' for this game. At the beginning I assumed
the top players were doing some magic path calculation that I was missing completely
but really it seems everyone used pretty simple heuristics to model each part of the game
(including myself) and then just did a ton of tuning.
In general there isn't a ton of high level strategic depth to the game beyond 'mine as much as you can as fast as you can', and most of 
the evaluation of moves for a given turtle can be done in a fairly small local context.

# Gathering and Core
Ships are assigned roles based on their current state. The roles are
gathering, returning, or end of game returning.

Gathering ships score each square according to a somewhat complicated 
cost function that essentially represents the halite / turn if that turtle
decided to path there, pickup most of the halite on that square and return to the nearest dropoff.


This function is where most of the magic happens.
Some interesting points: I incentive moving moving to new dropoffs (dropoffs that have a avg halite > 150).
In 1v1 I target enemy ships a lot more and in 4p I target ships with a decay (more worth it to collide ships later in the game).
I add a bonus for the number of friends in a radius of 5 minus the number of enemies in a radius of 5 if the square is inspired.

```C++
double GameMap::costfn(Ship *s, int to_cost, int home_cost, Position shipyard, Position dest, PlayerId pid, bool is_1v1,
                       int extra_turns, Game &g, double future_ship_val) {
    if (dest == shipyard) return 10000000;

    double bonus = 1;

    int halite = at(dest)->halite;
    int turns_to = calculate_distance(s->position, dest);
    int turns_back = calculate_distance(dest, shipyard);

    int avg_amount = avg_around_point(dest, 3);
    bonus = max(1.0, min(4.0, 3.0 * avg_amount / 150.0));

    int shipyard_bonus = avg_around_point(shipyard, 3);
    if (shipyard_bonus > 150) {
        bonus += 3;
    }

    int enemies = enemies_around_point(dest, 5);
    int friends = friends_around_point(dest, 5);

    int turns = fmax(1.0, turns_to + turns_back);
    if (!is_1v1) {
        turns = turns_to + turns_back;
    }

    if (at(dest)->occupied_by_not(pid)) {
        if (should_collide(dest, s)) {
            if (is_1v1) {
                halite += at(dest)->ship->halite;
            }
            if (!is_1v1) {
                halite += at(dest)->ship->halite;
                halite *= (0.5 + game->turn_number / ((double)constants::MAX_TURNS * 2.0));
            }
        } else {
            if (!is_1v1) {
                return 1000000;
            }
        }
    }

    bool inspired = false;

    if (is_inspired(dest, pid) || likely_inspired(dest, turns_to)) {
        if (is_1v1 && turns_to < 6) {
            bonus += 1 + (1 + friends - enemies);
            inspired = true;
        }
        else if (!is_1v1 && turns_to < 6) {
            bonus += 1 + (1 + friends - enemies);
            inspired = true;
        }
    }

    int curr_hal = s->halite;
    double out = -1000;
    int mined = 0;
    for (int i = 0; i<5; i++) {
        mined += halite * 0.25;
        if (inspired) {
            mined += halite * 0.5;
        }
        halite *= 0.75;
        if (mined + curr_hal > 1000) {
            mined = 1000 - curr_hal;
        }
        int c = max(0, mined - to_cost);
        double cout = (c) / ((double)1 + turns + i);
        out = max(cout, out);
    }

    bonus = fmax(bonus, 1);
    return -bonus * out;
}
```



Let each tile on the grid and each ship be nodes that form two bipartitions in a graph where the edges between the bipartitions are the cost of turtle X to go to square Y. Now the problem is one of bipartite matching where we want to maximize the gathering rate of our fleet. This can be done by the Hungarian algorithm which finds a min cost max matching efficiently.




# Returning ships
Returning ships take the min cost path (with min turns) path back to the nearest dropoff calculated using BFS. Returning ships will also pick up if the value of the square is over a certain threshold. Returning ships aggressively avoid enemy ships and try to path around them (again using BFS) but this pathing does no enemy prediction so there is possiblities that it could get stuck in a loop.

# Inspiration
Inspiration is huge, especially in 4p. From a game theory perspective inspiration in 4p is interesting. Similar to the non-aggression pact in halite 2, you want to sort of mutually inspire your partners. But I never really
had any fancy inspiration prediction (and from reading other post mortems this didn't
seem to be a common feature.) In 2p, I do take into account whether my turtles are
giving inspiration to the enemy, and the expected value in terms of halite/turn we
are giving if we try to collect at that square. I also encourage clumping of my turtles
since in general that gives more efficient inspiration.

# Collision avoidance
In general in 2p collisions are fairly simple. You want to take good collisions and want to avoid bad collisions. Returning ships will try to avoid making a move that could result in a collision at all costs. Some factors that go into collision avoidance is
whether I have more turtles in a radius of 3, whether I have a turtle that is at least
1 closer to collision destination, the amount of halite both turtles have.
If I deem a collision to be bad
There was some interesting meta evolving in 4p turtle collisions. There's a prisoners
dilemma. In the end though I just made my constraints for a 4p collision slightly more strict than 2p collisions.

Here is the main function that chooses whether a collision is net profitable or not.
```C++
bool GameMap::should_collide(Position position, Ship *ship, Ship *enemy) {
    bool is4p = game->players.size() == 4;
    if (enemy == nullptr) {
        enemy = at(position)->ship.get();
    }

    int possible_loss = 0;
    if (ship->position == position) {
        possible_loss = at(position)->halite;
    }

    if (at(position)->has_structure())
        return at(position)->structure->owner == constants::PID;

    if (enemy == nullptr) return true;

    int hal_on_square = at(position)->halite;
    int enemy_hal = hal_on_square + enemy->halite;
    if (ship->halite > 700) return false;
    if (possible_loss + enemy_hal < 300) return false;

    Ship *cenemy = get_closest_ship(position, game->getEnemies(), {ship, enemy});
    Ship *pal = get_closest_ship(position, {game->me}, {ship, enemy});

    if (cenemy == nullptr) return true;
    if (pal == nullptr) return false;

    int collision_hal = cenemy->halite + pal->halite + hal_on_square;

    // check if pal can gather
    int enemies = enemies_around_point(position, 4);
    int friends = friends_around_point(position, 4);
    if (friends >= enemies + 2) return true;

    int paldist = calculate_distance(pal->position, position);
    int enemydist = calculate_distance(cenemy->position, position);
    if (paldist > 2 + enemydist) return false;


    if (pal->halite + collision_hal > 1400) return false;

    if (ship->halite > possible_loss + enemy_hal)
        return false;

    if (friends >= enemies) {
        return true;
    }
    return false;
}
```

# Dropoffs
Dropoffs were the main improvement to my bot in the mid to late season.
I saw significant improvements once I implemented a system for ships to plan dropoffs
in the future and make gathering/returning decisions based on dropoffs that haven't
even been created yet. That being said my heuristic to actually plan the dropoff was pretty dumb. I planned a dropoff at the square with the max halite in a manhattan radius of 4 as long as it wasn't too close to enemies or an enemy dropoff.

# Dispatch
After each ship has a destination, they generate x random walks towards their destination
and take the walk that gives the best halite/turn. 
Each ship assigns a cost to each direction based on how badly it wants to move/not move there.
Since we never want to make a move where our turtles would collide (except for the end of the match), we can use the Hungarian algorithm again to resolve
the min cost set of moves for all our ships with a 1 turn lookahead while avoiding collisions completely.

Through coincidence several other competitors used something very similar. I recommend reading cowzow's post mortem as he describe
how this works in more detail.
https://github.com/dzou/halite3

# Ship spawning
Copy catting ship spawns in 2p seemed like a pretty good 'never be worse strategy'. I would permit overbuilding ships by about 4-5 over my opponent in 2p and use the
same hard constants that I used in 4p to stop spawning.

In 4p figuring out how many ships to spawn is a little trickier. ZanderShah and I tried
experimenting with a exponential moving average to estimate the value of a gathering ship (in terms of halite / turns)
at any given time if (remaining turns * value est > ~1500) it would be likely that a spawned ship could payoff itself. However in general this seemed to perform just as good as naive tactics. In the end I used a combination of this, mirroring my opponents and the total amount of halite on the map / the total number of ships.

# Tooling
I used flourine for offline replay viewing which was a big help. I also added 
debug visualizations using flog to visualize targetting and dropoff placement. I do
regret not investing more into tooling overall especially after seeing tooling used by other competitors.

Here is a screenshot from one of my local replays in fluorine setup.
![fluorine](https://i.imgur.com/9r4xxOU.png)

# Local testing
In general local testing was very good in this game. As I said before there is no real rock paper scissors element in this game
So if v151 beats v150 in your tests 90% of the time it's likely v151 will perform better on the ladder.

4p games are somewhat more complicated because of inspiration. If you test with 1 v151 and 3 v150, because the v150 bots
behave the same, they might get more inspiration because they'll mirror eachothers moves more often. My remedy for this was
to use two bots of the current generation and two bots of the previous generation.

I also didn't setup any cloud computing to run games, and I think this may have been useful for parameter tuning, but I found
it wasn't necessary with the rate of games that my bot got on the ladder.

Here is my stats for the playoffs.
Stats from mlombs fantastic site 
![stats](https://i.imgur.com/HdrNwFd.png)

My bot seemed to be better in 2p. I believe this is mostly because I was good
at taking aggressive collisions, and the collisions I took in 4p were less
valuable.


## Improvements for next season
Overall halite was a fantastic competition that I spent far too much time on.
I plan to play again next year. Again the main improvement I think I can make
for next year is invest into tooling early on.


## Halite III
This is the code used to run the bot I wrote for the Halite III AI competition
ran by two sigma. I came 7th overall and was the highest ranked undergraduate
student. 

This was the first AI competition i've ever participated in and had a lot of fun.

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
in Halite. Playing against the top players there would be games you would just lose and have no idea why you lost them.

There was never any 'secret sauce' for this game. At the beginning I assumed
the top players were doing some magic path calculation that I was missing completely
but really it seems everyone used pretty simple heuristics to model each part of the game
(including myself).
In general there isn't a ton of high level strategic depth to the game beyond 


# Gathering and Core
Ships are assigned roles based on their current state. The roles are
gathering, returning, or end of game returning.

Gathering ships score each square according to a somewhat complicated 
cost function that essentially represents the halite / turn if that turtle
decided to path their, pickup most of the halite and return to the nearest dropoff.

Let each tile on the grip and each ship be nodes that form two bipartitions in a graph where the edges between the bipartitions are the cost of turtle X to go to square Y. Now the problem is one of bipartite matching where we want to maximize the gathering rate of our fleet. This can be done by the Hungarian algorithm which finds a min cost max matching efficiently.

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

# Dropoffs
Dropoffs were the main improvement to my bot in the mid to late season.
I saw significant improvements once I implemented a system for ships to plan dropoffs
in the future and make gathering/returning decisions based on dropoffs that haven't
even been created yet. That being said my heuristic to actually plan the dropoff was pretty dumb. I planned a dropoff at the square with the max halite in a manhattan radius of 4 as long as it wasn't too close to enemies or an enemy dropoff.

# Dispatch
After each ship has a destination, they generate x random walks towards their destination
and take the walk that gives the best halite/turn. 
Each ship scores each direction by the amount they want to go there.
Since we never want to make a move where our turtles would collide (except for the end of the match) 

# Ship spawning
Copy catting ship spawns in 2p seemed like a pretty good 'never be worse strategy'.

In 4p figuring out how many ships to spawn is a little trickier. ZanderShah and I tried
experimenting with a exponential moving average to estimate the value of a gathering ship
at any given time and if that value was over a certain threshold than it would be likely that a spawned ship could payoff itself. However in general this seemed to perform just as good as naive tactics. In the end I used a combination of this, mirroring my opponents and the total amount of halite on the map / the total number of ships.

# Tooling
I used flourine for offline replay viewing which was a big help. I also added 
debug visualizations using flog to visualize targetting and dropoff placement. I do
regret not investing more into tooling overall especially after seeing tooling used by other competitors.

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


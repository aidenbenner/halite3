## Halite III
This is the code used to run the bot I wrote for the Halite III AI competition
ran by two sigma. I came 7th overall and was the highest ranked undergraduate
student. 

This was the first AI competition i've ever participated in and had a lot of fun.
The community surrounding the game was fantastic and very open and helpful.

Here is more information about the game https://halite.io/
And here is my player page https://halite.io/user/?user_id=1185

# Post-Mortem

Here is a screen cap of my bot in action.
![Halite](https://i.imgur.com/TmiKKkw.png)

Before reading you should read the overview of the rules for halite 3 from 
the page above. Halite 3 is a resource management game.

# Gathering and Core
Ships are assigned roles based on their current state. The roles are
gathering, returning, or end of game returning.

Gathering ships score each square according to a somewhat complicated 
cost function that essentially represents the halite / turn if that turtle
decided to path their, pickup most of the halite and return to the nearest dropoff.

Let each tile on the grip and each ship be nodes that form two bipartitions in a graph where the edges between the bipartitions are the cost of turtle X to go to square Y. Now the problem is one of bipartite matching where we want to maximize the gathering rate of our fleet. This can be done by the Hungarian algorithm which finds a min cost max matching efficiently.

# Returning ships
Returning ships take the min cost path (with min turns) path back to the nearest dropoff calculated using BFS. Returning ships will also pick up if the value of the square is over a certain threshold. Returning ships aggressively avoid enemy ships and try to path around them (again using BFS) but this pathing does no enemy prediction so there is possiblities that it could get stuck in a loop.

# Collision avoidance
In general in 2p collisions are fairly simple. You want to take good collisions and want to avoid bad collisions. Returning ships will try to avoid making a move that could result in a collision at all costs. If I deem a collision to be bad
There was some interesting meta evolving in 4p turtle collisions. There's a prisoners
dilemma. In the end though I just made my constraints for a 4p collision slightly more strict than 2p collisions.

# Dropoffs
Dropoffs were the main improvement to my bot in the mid to late season.
I saw significant improvements once I implemented a system for ships to plan dropoffs
in the future and make gathering/returning decisions based on dropoffs that haven't
even been created yet. That being said my heuristic to actually plan the dropoff was pretty dumb. I planned a dropoff at the square with the max halite in a manhattan radius of 4 as long as it wasn't too close to enemies or an enemy dropoff.

# Tooling

I used flourine for offline replay viewing which was a big help. I also added 
debug visualizations using flog to visualize targetting and dropoff placement. I do
regret not investing more into tooling overall especially after seeing tooling used by other competitors.


Stats page from mlombs fantastic site 
[stats](https://i.imgur.com/HdrNwFd.png)


Overall halite was a fantastic competition that I spent far too much time on.
I plan to play again next year.





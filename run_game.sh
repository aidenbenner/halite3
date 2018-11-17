#!/usr/bin/env bash

set -e

cmake .
make
./halite --replay-directory replays/ -vvv "./MyBot" "./bots/v33"
#./halite --replay-directory replays/ -vvv --width 48 --height 48  "./MyBot" "./benchmarks/RandomBot.py"  --seed 243
# ./halite --replay-directory replays/ -vvv "./MyBot" "./bots/AlexBot" 
#./halite --replay-directory replays/ -vvv --width 16 --height 16 "./MyBot" "./old/MyBot" 
#./halite --replay-directory replays/ -vvv  "./MyBot" "./old/MyBot" "./old/MyBot" "./old/MyBot" 

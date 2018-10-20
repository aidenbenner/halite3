#!/usr/bin/env bash

set -e

cmake .
make
./halite --replay-directory replays/ -vvv "./MyBot" "./old/MyBot" 
# ./halite --replay-directory replays/ -vvv --width 32 --height 32 "./MyBot" "./old/MyBot" 
./halite --replay-directory replays/ -vvv  "./MyBot" "./old/MyBot" "./old2/MyBot" "./old3/MyBot" 

#!/usr/bin/env python3
import random
import os
import sys

if os.system('cmake . && make') != 0:
    sys.exit()
args = set(sys.argv)

debug = '--debug' in args
no_drops = '--nodrop' in args
no_inspr =  '--noinspr' in args

bot_args = []
if debug:
    bot_args.append('--debug')
if no_drops:
    bot_args.append('--nodrop')
if no_inspr:
    bot_args.append('--noinspr')
bot_arg_str = ' '.join(bot_args)
print('bot args %s', bot_arg_str)

seed = random.randint(9900000,100000000)
seed -= seed % 10000
print('Running with seed %s' % seed)

os.system('./halite --replay-directory replays/ -vvv "./MyBot %s" "./bots/benchmark %s"' % (bot_arg_str, bot_arg_str))
os.system('./halite --replay-directory replays/ -vvv "./MyBot %s" "./MyBot %s" "./bots/benchmark %s" "./bots/benchmark %s"'
        % (bot_arg_str, bot_arg_str, bot_arg_str, bot_arg_str))
os.system("ps aux | grep -i './MyBot\\|./bots/' | awk '{print $2}' | xargs sudo kill -9")


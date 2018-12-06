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
small =  '--small' in args
ssmall =  '--ssmall' in args
medium =  '--medium' in args
p4 = '--4p' in args

bot_args = []
if debug:
    bot_args.append('--debug')
if no_drops:
    bot_args.append('--nodrop')
if no_inspr:
    bot_args.append('--noinspr')

width = random.choice([32,48,56,64])
if small:
    width = 32
if medium:
    width = 48
if ssmall:
    width = 20
bot_arg_str = ' '.join(bot_args)
print('bot args %s', bot_arg_str)

seed = random.randint(9900000,100000000)
seed -= seed % 10000
print('Running with seed %s' % seed)

if p4:
    os.system('./halite --replay-directory replays/ -vvv --width %s --height %s "./MyBot %s" "./bots/benchmark %s" "./bots/benchmark %s" "./bots/benchmark %s"' % (width, width, bot_arg_str, bot_arg_str, bot_arg_str, bot_arg_str))
else:
    os.system('./halite --replay-directory replays/ -vvv --width %s --height %s "./MyBot %s" "./bots/benchmark %s"' % (width, width, bot_arg_str, bot_arg_str))
os.system("ps aux | grep -i './MyBot\\|./bots/' | awk '{print $2}' | xargs sudo kill -9")


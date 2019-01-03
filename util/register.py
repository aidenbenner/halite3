#!/usr/bin/env python3
import os

print('registering bots')
for f in os.listdir('./bots'):
    print('registering ./bots/' + f)
    #os.system('hlt gym register %s ./bots/%s' % (f,f))
os.system('hlt gym register current ./MyBot')
os.system('hlt gym register bench ./bots/benchmark')
os.system('hlt gym register v107 ./bots/v107')
os.system('hlt gym register v108 ./bots/v108')
os.system('hlt gym register v109 ./bots/v109')

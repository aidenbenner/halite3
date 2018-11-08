#!/usr/bin/env python3
import os

print('registering bots')
for f in os.listdir('./bots'):
    print('registering ./bots/' + f)
    os.system('hlt gym register %s ./bots/%s' % (f,f))
os.system('hlt gym register current ./MyBot')

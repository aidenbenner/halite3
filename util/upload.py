#!/usr/bin/env python3

import os

print('Version #')
version = int(input())

result = os.system('make')
if result != 0:
    print("Did not compile")
    sys.exit()

os.system('zip MyBot.zip MyBot.cpp hlt/* CMakeLists.txt cmake_install.cmake Makefile CMakeFiles')
os.system('hlt bot -b ./MyBot.zip upload')
os.system('cp MyBot ./bots/v%s' % version)
os.system('hlt gym register %s ./bots/v%s' % (version, version))

print('Did you commit? y/n')
response = input()

if response == 'y':
    os.system('git tag -a v%s' % version)
else:
    print("please tag the branch")

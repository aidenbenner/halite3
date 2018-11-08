#!/usr/bin/env python3

import os

print('which version is this?')
version = int(input())

os.system('zip MyBot.zip MyBot.cpp hlt/* CMakeLists.txt cmake_install.cmake Makefile CMakeFiles')
os.system('hlt bot -b MyBot.zip upload')
os.system('cp MyBot ./bots/' + version)
os.system('hlt gym register %s ./bots/%s' (version, version))

print('remember to commit')

#!/bin/bash

gcc src/key_generator.c -g -O3 -o src/key_generator
apt-get install uuid-dev

autoreconf --install
./configure CFLAGS='-g -O3'

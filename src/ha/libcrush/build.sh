#!/bin/bash

set -ex
mkdir -p build
(
   #
   # building
   #
   cd build
   cmake ..
   make
   make install
)

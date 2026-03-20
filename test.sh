#!/bin/bash
cd build
cmake ..
make
./chip_image_detect
cd ..
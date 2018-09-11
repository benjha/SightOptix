#!/bin/bash
export LD_LIBRARY_PATH=/home/benjha/Programs/lib:/home/benjha/Programs/NVIDIA-OptiX-SDK-5.0.1-linux64/lib64:$LD_LIBRARY_PATH
./SightOptix $1 $2

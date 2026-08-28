#!/bin/sh

cmake --preset default
cmake --build --preset default
ctest --preset default

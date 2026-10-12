@echo off
if not exist "build\" (
    cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_CXX_COMPILER=g++
)
cmake --build build

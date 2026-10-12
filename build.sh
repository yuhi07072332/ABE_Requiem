if [ ! -d "./build" ]; then
    cmake -B build -S.
fi
cmake --build build

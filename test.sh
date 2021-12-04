#!/bin/bash

# Clean the existing build folder
rm -r ./build

# Build the latest program
mkdir -p build
cmake -S. -B./build
cmake --build ./build

echo "--- RUNNING TESTS ... ---"
diff -u ./data/01.a <(./build/ex3 < ./data/01)
diff -u ./data/02.a <(./build/ex3 < ./data/02)
diff -u ./data/03.a <(./build/ex3 < ./data/03)
diff -u ./data/04.a <(./build/ex3 < ./data/04)
diff -u ./data/05.a <(./build/ex3 < ./data/05)
echo "--- DONE ! ---"

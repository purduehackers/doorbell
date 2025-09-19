mkdir -p build

cmake -B build/ .

make -C build/ app-flash

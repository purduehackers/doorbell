mkdir -p build

cmake -B build/ .

make -C build/ partition-table-flash
make -C build/ bootloader-flash

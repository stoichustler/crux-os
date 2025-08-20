OUTPUT=build-arm64

cmake -B$OUTPUT -GNinja \
  -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DCMAKE_INSTALL_PREFIX=../aarch64 \
  -DCMAKE_TOOLCHAIN_FILE=aarch64-none-elf.cmake \
  -Dmultilib=off \
  -Dtests=off \

cmake --build $OUTPUT
cmake --install $OUTPUT

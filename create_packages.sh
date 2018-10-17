#! /usr/bin/env bash

echo "Create Linux Package."

mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make package
cd ..

echo "Create Windows x64 Package."

docker run --rm dockcross/windows-x64 > ./dockcross-windows-x64
chmod +x dockcross-windows-x64
./dockcross-windows-x64 cmake -Bbuild-windows-x64 -H. -DCMAKE_BUILD_TYPE=Release -DUSE_LINGELING=Off
./dockcross-windows-x64 make -C build-windows-x64 package

echo "Create Windows x32 Package."

docker run --rm dockcross/windows-x86 > ./dockcross-windows-x32
chmod +x dockcross-windows-x32
./dockcross-windows-x32 cmake -Bbuild-windows-x32 -H. -DCMAKE_BUILD_TYPE=Release -DUSE_LINGELING=Off
./dockcross-windows-x32 make -C build-windows-x32 package

echo "Finished Creating Packages, copy to packages subfolder."
mkdir -p packages

cp ./build/packages/* ./packages
cp ./build-windows-x64/packages/* ./packages
cp ./build-windows-x32/packages/* ./packages

echo "Finished Creating Packages! Created:";

ls -1 ./packages

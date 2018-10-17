#! /usr/bin/env bash

echo "Create Linux Package."

docker run --rm dockcross/linux-x64 > ./dockcross-linux-x64
chmod +x dockcross-linux-x64
./dockcross-linux-x64 cmake -Bbuild-linux-x64 -H. -DCMAKE_BUILD_TYPE=Release
./dockcross-linux-x64 make -C build-linux-x64 package

echo "Create Windows Package."

docker run --rm dockcross/windows-x64 > ./dockcross-windows-x64
chmod +x dockcross-windows-x64
./dockcross-windows-x64 cmake -Bbuild-windows-x64 -H. -DCMAKE_BUILD_TYPE=Release -DUSE_LINGELING=Off
./dockcross-windows-x64 make -C build-windows-x64 package

echo "Finished Creating Packages, copy to packages subfolder."
mkdir -p packages

cp ./build-linux-x64/packages/* ./packages
cp ./build-windows-x64/packages/* ./packages

echo "Finished Creating Packages! Created:";

ls -1 ./packages

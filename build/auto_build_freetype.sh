#!/bin/bash

home=`pwd`
echo workdir:$home

mkdir -p third_party
cd third_party

FREE_IMAGE_SRC_DIR=./freetype-2.10.4

##download opencv
if [ ! -d "$FREE_IMAGE_SRC_DIR" ]; then
    wget http://nchc.dl.sourceforge.net/project/freetype/freetype2/2.10.4/freetype-2.10.4.tar.xz
    tar -xf freetype-2.10.4.tar.xz
fi

cd freetype-2.10.4/builds

cmake -DCMAKE_INSTALL_PREFIX=../install ..
make -j10 && make install

cd ../install
libpath=`find ./ -name "libfreetype.a"`

# copy header and lib

echo "copy header and lib ..."
cd $home
mkdir -p $home/include/freetype
mkdir -p $home/lib/freetype
cp -rf $home/third_party/freetype-2.10.4/install/include/freetype2/*  $home/include/freetype/
cp -rf $home/third_party/freetype-2.10.4/install/${libpath} $home/lib/freetype/
echo "copy header and lib success"

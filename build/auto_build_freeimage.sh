#!/bin/bash

home=`pwd`
echo workdir:$home

mkdir -p third_party
cd third_party

FREE_IMAGE_SRC_DIR=./FreeImage

##download opencv
if [ ! -d "$FREE_IMAGE_SRC_DIR" ]; then
    wget http://udomain.dl.sourceforge.net/project/freeimage/Source%20Distribution/3.18.0/FreeImage3180.zip
    unzip FreeImage3180.zip
fi

cd FreeImage

make -j10

# copy header and lib

echo "copy header and lib ..."
cd $home
mkdir -p $home/include/FreeImage
mkdir -p $home/lib/FreeImage
cp -rf $home/third_party/FreeImage/Dist/FreeImage.h  $home/include/FreeImage/
cp -rf $home/third_party/FreeImage//Dist/libfreeimage.a  $home/lib/FreeImage/
echo "copy header and lib success"

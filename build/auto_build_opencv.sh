#!/bin/bash

home=`pwd`
echo workdir:$home

mkdir -p third_party
cd third_party

OPENCV_SRC_DIR=./opencv

##download opencv
if [ ! -d "$OPENCV_SRC_DIR" ]; then
    git clone https://github.com/opencv/opencv.git
fi

##切换到3.2.0的tag
cd opencv
git checkout 3.2.0


opencv_tag_name=`git describe --abbrev=0 --tags`

if [[ "$opencv_tag_name" != "3.2.0" ]]; then
    echo "we need opencv tag 3.2.0, tag name is $jsoncpp_tag_name"
    exit
fi


mkdir -p build
cd build

/usr/bin/cmake -DBUILD_DOCS=off \
    -DBUILD_SHARED_LIBS=off \
    -DBUILD_FAT_JAVA_LIB=off \
    -DBUILD_TESTS=off \
    -DBUILD_TIFF=on \
    -DBUILD_JASPER=on \
    -DBUILD_JPEG=on \
    -DBUILD_PNG=on \
    -DBUILD_ZLIB=on \
    -DBUILD_OPENEXR=off \
    -DBUILD_opencv_apps=off \
    -DBUILD_opencv_calib3d=off \
    -DBUILD_opencv_contrib=off \
    -DBUILD_opencv_features2d=off \
    -DBUILD_opencv_flann=off \
    -DBUILD_opencv_gpu=off \
    -DBUILD_opencv_java=off \
    -DBUILD_opencv_legacy=off \
    -DBUILD_opencv_ml=on \
    -DBUILD_opencv_nonfree=off \
    -DBUILD_opencv_objdetect=off \
    -DBUILD_opencv_ocl=off \
    -DBUILD_opencv_photo=off \
    -DBUILD_opencv_python=off \
    -DBUILD_opencv_stitching=off \
    -DBUILD_opencv_superres=off \
    -DBUILD_opencv_ts=off \
    -DBUILD_opencv_video=off \
    -DBUILD_opencv_videostab=off \
    -DBUILD_opencv_world=off \
    -DBUILD_opencv_lengcy=off \
    -DBUILD_opencv_lengcy=off \
    -DWITH_1394=off \
    -DWITH_EIGEN=off \
    -DWITH_FFMPEG=off \
    -DWITH_GIGEAPI=off \
    -DWITH_GSTREAMER=off \
    -DWITH_GTK=off \
    -DWITH_PVAPI=off \
    -DWITH_V4L=off \
    -DWITH_LIBV4L=off \
    -DWITH_CUDA=off \
    -DWITH_CUFFT=off \
    -DWITH_OPENCL=off \
    -DWITH_OPENCLAMDBLAS=off \
    -DWITH_OPENCLAMDFFT=off \
    -DCMAKE_BUILD_TYPE=RELEASE  \
    -DCMAKE_INSTALL_PREFIX=../install \
    ../

make -j10  && make install

# copy header and lib

echo "copy header and lib ..."
cd $home
mkdir -p $home/include
mkdir -p $home/lib/opencv
cp -rf $home/third_party/opencv/install/include/*  $home/include/
cp -rf $home/third_party/opencv/install/lib/*  $home/lib/opencv/
cp -rf $home/third_party/opencv/install/share/OpenCV/3rdparty/lib/*  $home/lib/
echo "copy header and lib success"

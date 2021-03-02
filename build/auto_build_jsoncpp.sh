#!/bin/bash

home=`pwd`
echo workdir:$home

mkdir -p third_party
cd third_party

JSON_CPP_DIR=./jsoncpp

##download opencv
if [ ! -d "$JSON_CPP_DIR" ]; then
    git clone https://github.com/open-source-parsers/jsoncpp.git
fi

cd jsoncpp

git checkout 0.y.z

jsoncpp_tag_name=`git branch|grep \*`

if [[ "$jsoncpp_tag_name" =~ "\* 0.y.z" ]]; then
    echo "we need jsoncpp branch * 0.y.z, branch name is $jsoncpp_tag_name"
    exit
fi

git checkout 0.y.z

mkdir -p build
cd build

cmake -DCMAKE_INSTALL_PREFIX=../install ../

make -j10  && make install

# copy header and lib

echo "copy header and lib ..."
cd $home
mkdir -p $home/include
mkdir -p $home/lib/jsoncpp
cp -rf $home/third_party/jsoncpp/install/include/*  $home/include/
cp -rf $home/third_party/jsoncpp/install/lib/x86_64-linux-gnu/libjsoncpp.a  $home/lib/jsoncpp/
echo "copy header and lib success"

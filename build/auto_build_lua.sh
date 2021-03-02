#!/bin/bash

home=`pwd`
echo workdir:$home

mkdir -p third_party
cd third_party

LUA_DIR=./lua5.1

##download opencv
if [ ! -d "$LUA_DIR" ]; then
    wget  http://netactuate.dl.sourceforge.net/project/luabinaries/5.1.5/Linux%20Libraries/lua-5.1.5_Linux313_64_lib.tar.gz
    mkdir lua5.1
    tar -zxvf lua-5.1.5_Linux313_64_lib.tar.gz -C ./lua5.1
fi

cd lua5.1

# copy header and lib

echo "copy header and lib ..."
cd $home
mkdir -p $home/include/lua5.1
mkdir -p $home/lib/lua5.1
cp -rf $home/third_party/lua5.1/include/*  $home/include/lua5.1/
cp -rf $home/third_party/lua5.1/liblua5.1.a  $home/lib/lua5.1/
echo "copy header and lib success"

cd third_party
LUA_BIND_DIR=./luabind

if [ ! -d "$LUA_BIND_DIR" ]; then
    git clone https://github.com/luabind/luabind.git
fi

cd luabind
export LUA_PATH=$home/third_party/lua5.1:$LUA_PATH
#env | grep LUA
#pwd

bjam link=static release

echo "copy header and lib ..."
cd $home
mkdir -p $home/include/luabind
mkdir -p $home/lib/luabind
cp -rf $home/third_party/luabind/luabind/*  $home/include/luabind/
cp -rf $home/third_party/luabind/bin/gcc-4.6/release/link-static/libluabind.a  $home/lib/luabind
echo "copy header and lib success"

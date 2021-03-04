#!/bin/bash

home=`pwd`
echo workdir:$home

mkdir -p third_party
cd third_party

LUA_DIR=./lua5.1

##download lua
if [ ! -d "$LUA_DIR" ]; then
    wget  http://netactuate.dl.sourceforge.net/project/luabinaries/5.1.5/Linux%20Libraries/lua-5.1.5_Linux313_64_lib.tar.gz
    mkdir lua5.1
    tar -zxvf lua-5.1.5_Linux313_64_lib.tar.gz -C ./lua5.1
fi

cd lua5.1

# copy header and lib

echo "copy header and lib ..."
mkdir lib
cp -rf lib* ./lib
cd $home
mkdir -p $home/include/lua5.1
mkdir -p $home/lib/lua5.1
cp -rf $home/third_party/lua5.1/include/*  $home/include/lua5.1/
cp -rf $home/third_party/lua5.1/liblua5.1.a  $home/lib/lua5.1/
echo "copy header and lib success"


#download bjam todo delete
echo "dowload bjam"
cd $home/third_party
BJAM_DIR=./bjam
if [ ! -d "$BJAM_DIR" ]; then
    wget "http://sourceforge.net/projects/boost/files/boost-jam/3.1.18/boost-jam-3.1.18.tgz"
    tar -zxvf boost-jam-3.1.18.tgz
    mv boost-jam-3.1.18 bjam
fi
cd bjam
./build.sh
BJAM_PATH=`find ./ -name bjam`
LOCAL_PATH=`pwd`
BJAM_BIN=$LOCAL_PATH/$BJAM_PATH


#download boost 1.34
echo "dowload boost 1.48.1"
cd $home/third_party
BOOST_DIR=./boost
if [ ! -d "$BOOST_DIR" ]; then
	wget "https://sourceforge.net/projects/boost/files/boost/1.48.0/boost_1_48_0.tar.gz"
    tar -zxvf boost_1_48_0.tar.gz
    mv boost_1_48_0 boost
fi

echo "build boost"
cd boost
./bootstrap.sh
#./b2 install

echo "copy header and lib ..."
cd $home
mkdir -p $home/include/boost
mkdir -p $home/lib/boost
cp -rf $home/third_party/boost/boost/*  $home/include/boost/
cp -rf $home/third_party/boost/stage/lib/*  $home/lib/boost
echo "copy header and lib success"


cd $home/third_party
LUA_BIND_DIR=./luabind

if [ ! -d "$LUA_BIND_DIR" ]; then
    git clone https://github.com/luabind/luabind.git
fi

cd luabind
export LUA_PATH=$home/third_party/lua5.1:$LUA_PATH
#env | grep LUA
#pwd

#bjam link=static release
BOOST_BJAM=$home/third_party/boost/bjam
export BOOST_ROOT=$home/third_party/boost
export BOOST_BUILD_ROOT=$home/third_party/boost
export LUA_PATH=$home/third_party/lua5.1
$BOOST_BJAM link=static release
LUABIND_LIB=`find ./ -name libluabind.a`

echo "copy header and lib ..."
cd $home
mkdir -p $home/include/luabind
mkdir -p $home/lib/luabind
cp -rf $home/third_party/luabind/luabind/*  $home/include/luabind/
cp -rf $home/third_party/luabind/$LUABIND_LIB  $home/lib/luabind
echo "copy header and lib success"

#!/bin/bash
dir=`pwd`
home=$dir/third_party
mkdir -p $home
echo workdir:$home 

#download srs
echo "download srs..."
cd $home
SRS_DIR=./srs
if [ ! -d "$SRS_DIR" ]; then
    git clone https://github.com/ossrs/srs.git
fi
cd $home
echo "download srs success"

#compile
echo "compile srs"
cd $home/srs/trunk
./configure --prefix=$home/srs/trunk
make -j 10
cd $home

##copy header and lib
echo "copy header and lib ..."
cd $home/srs/trunk
SRS_LIBRTMP_PATH=`find ./ -name srs_librtmp.a`
SRS_LIBRTMP_INCLUDE_PATH=`find ./ -name srs_librtmp.hpp`
echo "srsrtmplib:"$SRS_LIBRTMP_PATH
echo "srsrtmpinclude:"$SRS_LIBRTMP_INCLUDE_PATH
mkdir -p $dir/include/srs
mkdir -p $dir/lib/srs

cp -rf $SRS_LIBRTMP_PATH $dir/lib/srs
cp -rf $SRS_LIBRTMP_INCLUDE_PATH $dir/include/srs
SSL_LIB=`find ./ -name libssl.a|grep release`
CRYPTO_LIB=`find ./ -name libcrypto.a|grep release`
echo "ssllib:"$SSL_LIB
echo "cryptolib:"$CRYPTO_LIB
cp -rf $SSL_LIB $dir/lib/srs
cp -rf $CRYPTO_LIB $dir/lib/srs

echo "copy header and lib success"

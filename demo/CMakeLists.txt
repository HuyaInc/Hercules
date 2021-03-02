cmake_minimum_required(VERSION 2.8)
project(mixdemo)

include_directories(
    ../src/common
    ../src/codec
    ../src/mixer
    ../src/lua
    ../src/core
    ./
    ../include
    ../include/jsoncpp
    )

link_directories(
    ${PROJECT_SOURCE_DIR}/../lib
    ${PROJECT_SOURCE_DIR}/../lib/freetype
    ${PROJECT_SOURCE_DIR}/../lib/opencv
    ${PROJECT_SOURCE_DIR}/../lib/lua5.1
    ${PROJECT_SOURCE_DIR}/../lib/jsoncpp
    ${PROJECT_SOURCE_DIR}/../lib/FreeImage
    ${PROJECT_SOURCE_DIR}/../lib/luabind
    ${PROJECT_SOURCE_DIR}/../lib/srs
    )

ADD_DEFINITIONS(-Wno-invalid-offsetof -Wextra -Wall -Wno-reorder -Wno-strict-aliasing -std=c++0x -fPIC -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS -DHUYA_RECV_USE_DEQUE -DMIX_SDK -g)

add_executable(
    mixdemo
    main.cpp
    InFlvStream.cpp
    )

find_library(SRS_LIB
    srs_librtmp.a
    PATHS ${PROJECT_SOURCE_DIR}/../lib/srs
    NO_DEFAULT_PATH)

target_link_libraries(mixdemo
    libmixsdk.a
    libutil.a
    libavformat.a
    libavcodec.a
    libavutil.a
    libswresample.a
    libswscale.a
    libopencv_ml.a
    libopencv_imgcodecs.a
    liblibpng.a
    liblibjpeg.a
    libopencv_imgproc.a
    libopencv_core.a
    libopencv_highgui.a
    libippicv.a
    libfreeimage.a
    libtcmalloc.a
    libx264.a
    liblibjasper.a
    libIlmImf.a
    libfreetype.a
    libjsoncpp.a
    liblua5.1.a
    libluabind.a
    libfdk-aac.a
    ${SRS_LIB}
    #libsrs_librtmp.a
    pthread
    dl
    z
    m
    bz2
    rt
    curl
    ssl
    crypto
    lzma
    )


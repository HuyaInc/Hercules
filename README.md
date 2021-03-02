	 _   _                     _           
	| | | | ___ _ __ ___ _   _| | ___  ___ 
	| |_| |/ _ \ '__/ __| | | | |/ _ \/ __|
	|  _  |  __/ | | (__| |_| | |  __/\__ \
	|_| |_|\___|_|  \___|\__,_|_|\___||___/
 



# 简介

Hercules名字来自于是古希腊神话中的大力神，传说完成过12项“不可能完成的任务”，希望我们该项目的灵活架构可以完成更多的“不可能完成的任务”。   
该项目提供一种脚本形式的控制画面布局和混音的控制方式，利用可热更新的lua脚本来完成基础接口的封装，再用json的方式灵活控制调用参数来达到想要的画面效果。   
目前主要功能是混画和混音，该框架在虎牙内部，各大核心业务都在使用，颇受欢迎，基于该框架部署运行的服务节点规模达到数千个，支持的业务量每天在数万个。   

# 特点
    
    1.强大的混画，混音功能，输入支持音视频，图片，文字，图片序列，gif等，输出支持本地文件，rtmp url。
    2.c++层封装了编解码，混画相关的基础api，lua层负责逻辑代码编写。
    3.使用lua和json搭配，更方便的进行混画和混音的控制。

# 编译

    1.下载代码
    2.mkdir bin
    3.cd bin
    4.cmake -DCMAKE_BUILD_TYPE=Debug/Release  -DADD_THIRD_PARTY="ON" path
    ADD_THIRD_PARTY选项用于生成依赖的第三方库
    主要包含以下：
    ffmpeg
    opencv
    freeimage
    x264
    fdk_aac
    freetype
    libjsoncpp
    openssl
    srs
    5.make && make install,这里会生成mixsdk.a文件和相关依赖头文件
    6.cd demo
    7.mkdir bin
    8.cd bin
    9.cmake ..
    10.make生成mixdemo文件

# 使用
* [使用方法](demo/README.md)

demo根据不同的config产生不同的混画效果，demo使用原始视频如下

![](demo/doc/original.gif)

目前configs中提供了以下几种混画效果

| config | 效果 | 效果图 |
|--------|------|--------|
| cut_down.json | 视频裁剪一半 | ![](demo/doc/cut_down.gif) |
| scale_down.json | 视频缩小一半 | ![](demo/doc/scale_down.gif) |
| concat.json | 视频拼接 | ![](demo/doc/concat.gif) |
| logo.json | 添加图片 | ![](demo/doc/logo.gif) |
| text.json | 添加文字 | ![](demo/doc/text.gif) |
| property_animation.json | 基于形状位置变化的动画 | ![](demo/doc/property_animation.gif) |
| frame_animation.json | 基于多帧图片的动画 | ![](demo/doc/frame_animation.gif) |

# 说明

    1.生成的mixsdk.a文件可以嵌入到自己的应用中。
    2.c++只封装了基本的编解码混画功能，逻辑在lua中实现。
    3.目前只提供了简单的demo，可以通过修改输入json在lua中实现各种复杂的逻辑和效果。
    4.关于混音，目前只实现了简单的混音，待完善。
	
# 支持平台

    目前只支持linux。




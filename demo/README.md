## 简介
这是一个使用Hercules SDK创建的demo工具，它可以根据不同的配置实现不同的功能

## 目录结构
demo目录下有 configs 和 data 两个目录

configs 每一个json文件都展示不同的混画效果
data 包含混画需要用到的素材，例如字体文件，图片，视频等


## 构建
本demo使用cmake构建工具。

**注意**: 构建之前必须要先编译Hercules的**MixSdk**库

然后进入demo目录 输入以下指令构建demo

```sh
mkdir bin
cd bin
cmake ..
make
```

此时在bin目录中将看到mixdemo二进制文件

## 使用
demo根据不同的config产生不同的混画效果，demo使用原始视频如下

![](doc/original.gif)

目前configs中提供了以下几种混画效果

| config | 效果 | 效果图 |
|--------|------|--------|
| cut_down.json | 视频裁剪一半 | ![](doc/cut_down.gif) |
| scale_down.json | 视频缩小一半 | ![](doc/scale_down.gif) |
| concat.json | 视频拼接 | ![](doc/concat.gif) |
| logo.json | 添加图片 | ![](doc/logo.gif) |
| text.json | 添加文字 | ![](doc/text.gif) |
| property_animation.json | 基于形状位置变化的动画 | ![](doc/property_animation.gif) |
| frame_animation.json | 基于多帧图片的动画 | ![](doc/frame_animation.gif) |

demo 的 命令格式为
> ./mixdemo configfile timeout 

其中 
	configfile 就是上述的json文件
	timeout 指定需要混画的时间

示例如下
> ./mixdemo ../config/cut_down.json 5


## json详解

### task_type && task_file

> **task_type** 指定使用的task文件的格式
>
> **task_file** 指定混画引擎，此demo使用Hercules提供的默认引擎

### src && dst

> **src** 和 **dst** 均为demo自定义的字段，用于指定输入和输出视频的文件
> 
> 目前demo只支持flv文件

### input_stream_list && out_stream

> **intput_stream_list** 指定所有输入元素，可以包括
> 1. 流
> 2. 图片
> 3. 文本
> 4. 动画
> 
> **out_stream** 指定输出流的信息，主要是音视频编码相关信息

### crop_rect && put_rect

> **crop_rect** 指定从数据源中裁剪的位置和大小
> 
> **put_rect** 指定数据放置在输出中的位置和大小
> 
> **注意**: 
> 
> 1. Hercules采用的坐标系为 左上角为(0,0)，向下y轴和向右x轴<br>
> 所以 top, left 的值小于 right, bottom
> 
> 2. 当**put_rect**和**crop_rect**的大小不一致时<br>
> Hercules会自动进行缩放以填满**put_rect**区域

### z_order

> **z_order** 指定该数据的图层，值大的处于顶层

### animation &&  group_container

> 由于 **huya.lua** 的引擎限制，动画需要放置于 `"type":"group_container"`的**input_stream_list**中
> 
> **group_container** 有自己的**input_list** 和 **out_put**
> 
> **input_list** 可以放入多个动画进行组合
> 
> **oupt_put** 指定组合动画的最终位置和大小，同时支持缩放


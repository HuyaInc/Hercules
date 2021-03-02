-- Copyright 2021 HUYA
--
-- Licensed under the Apache License, Version 2.0 (the "License");
-- you may not use this file except in compliance with the License.
-- You may obtain a copy of the License at
--
--     http://www.apache.org/licenses/LICENSE-2.0
--
-- Unless required by applicable law or agreed to in writing, software
-- distributed under the License is distributed on an "AS IS" BASIS,
-- WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
-- See the License for the specific language governing permissions and
-- limitations under the License.

package.path = getCWD() .. "/../../src/lua/?.lua;" .. package.path
current_working_dir = getCWD()
image_dir = current_working_dir .. '/../data/image'
font_dir =  current_working_dir .. '/../data/font'

debug_flag = debug_flag or false

start_ms_ = start_ms_ or getRunMs32()
now_ms_ = now_ms_ or start_ms

function getImageDir()
    return image_dir
end

function getFontDir()
    return font_dir
end

-- backgroup y u v
_G.bg_y = 237
_G.bg_u = 128
_G.bg_v = 128

_G.audio_frame_id = 0

_G.pre_input_streamname_list = _G.pre_input_streamname_list or {}
_G.cur_input_streamname_list = _G.cur_input_streamname_list or {}

_G.dump_file_index = _G.dump_file_index or 0
_G.dump_file_interval = 0
_G.dump_file_tick = _G.dump_file_tick or 0

_G.dump_pk_index = _G.dump_pk_index or 0
_G.dump_pk_interval = 0
_G.dump_pk_tick = _G.dump_pk_tick or 0
_G.dump_pk_first = false

_G.painter = _G.painter
_G.streamlist = _G.streamlist or {}
_G.imagelist = _G.imagelist or {}
_G.sort_function = _G.sort_function

_G.state_trace_time_ms = _G.state_trace_time_ms

_G.job_key = _G.job_key
_G.out_stream_name = _G.out_stream_name
_G.out_uid = _G.out_uid
_G.appid = 100

_G.mix_audio_tick = _G.mix_audio_tick or 0

_G.mix_video_tick = _G.mix_video_tick or 0
_G.mix_video_init_tick = _G.mix_video_init_tick or 0

_G.pre_out_audio_dts = _G.pre_out_audio_dts or 0
_G.pre_out_video_dts = _G.pre_out_video_dts or 0

_G.pre_mix_audio_ms = _G.pre_mix_audio_ms or 0
_G.pre_mix_video_ms = _G.pre_mix_video_ms or 0

_G.sixteen_nine = _G.sixteen_nine
_G.nine_sixteen = _G.nine_sixteen

font_file = font_file

_G.painter = _G.painter

_G.onPull = _G.onPull or {}
_G.onPush = _G.onPush or {}
_G.onDown = _G.onDown or {}

function diffTable(pre, cur)
    no_found = {}
    for k, v in pairs(pre) do
        found = 0
        for ck, cv in pairs(cur) do
            if cv == v then
                found = 1
            end
        end
        if found == 0 then
            table.insert(no_found, v)
        end
    end

    for k, v in pairs(no_found) do
    end

    return no_found
end

function duration_ms()
    return now_ms_ - start_ms_
end

function update_ms()
    now_ms_ = getRunMs32()
end

function now_ms()
    return now_ms_
end


function LogTableWithIndent(indent, table)
    if not debug_flag then
        return
    end

    indent = indent .. '\t'
    for key, value in pairs(table) do
        if type(value) == 'table' then
            LOG(indent .. '"' .. key .. '"{')
            LogTableWithIndent(indent, value)
            LOG(indent .. '}')
        else
            if value ~= nil then
                t = type(value)
                if t == "string" then
                    LOG(indent .. '"' .. key .. '":"' .. value .. '"')
                else
                    LOG(indent .. ' ' .. type(value))
                end
            end
        end
    end
    indent = string.sub(indent, 1, string.len(indent) - 1)
end

function LOGTable(table)
    for key, value in pairs(table) do
        if type(value) == 'table' then
            LOGTable(value)
        else
            LOG('"' .. key .. '":"' .. value .. '"')
        end
    end
end

function sortList(left, right)
    if left == nil or left.z_order == nil then
        return false
    end
    if right == nil or right.z_order == nil then
        return true
    end
    if tonumber(left.z_order) < tonumber(right.z_order) then
        return true
    else
        return false
    end
end

-- _ANIM_
local _ANIM_ = nil
---[[ cpp
local function InitAnim()
 local ok, anim = pcall(require, "animation")
 if ok then
  LOG('lzk animation model init success')
  _ANIM_ = anim
  return true
 else
  LOG('lzk animation model init fail')
 end
 return false
end
InitAnim()
--]]

function start(table)
    LOG("lua start")
    update_ms()
    LogTableWithIndent('', table)
    resourceInit(table)
    addPushStream(table.out_stream)
    jobInit(table.input_stream_list)
    LOG("lua start complete")
end

function updateBgColor(table)
    if table ~= nil and table.background_color ~= nil and #table.background_color==9 then
        s = table.background_color
        r = tonumber(string.sub(s, 4, 5), 16)
        g = tonumber(string.sub(s, 6, 7), 16)
        b = tonumber(string.sub(s, 8, 9), 16)
        if r >=0 and r <=255  and g >=0 and g <=255 and b >= 0 and b <= 255 then
            RGB2YUV(r,g,b)
        end
    end
end

function resourceInit(table)
    _G.pre_input_streamname_list = {}
    _G.cur_input_streamname_list = {}
    _G.painter = OpenCVOperator()

    _G.streamlist = {}
    _G.imagelist = {}
    _G.sort_function = sortList

    if _ANIM_ then
        _G.sort_function = _ANIM_.sortComp
    end

    _G.state_trace_time_ms = now_ms()

    _G.job_key = table.task_id
    _G.out_stream_name = table.out_stream.stream_name

    _G.mix_audio_tick = 0

    _G.mix_video_tick = 0
    _G.mix_video_init_tick = 0

    _G.pre_out_audio_dts = 0
    _G.pre_out_video_dts = 0

    _G.pre_mix_audio_ms = 0
    _G.pre_mix_video_ms = 0

    _G.sixteen_nine = 16.0 / 9.0
    _G.nine_sixteen = 9.0 / 16.0

    font_file = getFontDir() .. '/msyhl.ttc'
    LOG('setFont')
    _G.painter:setFontType(font_file)
end

function getAudioMixer(name)
    return _G.onPush[name].audio_mixer_wrapper:getRawPtr()
end

function fillPushDefaultArgs(value)
    video_default_codec = { width = 1920, height = 1080, kbps = 2000, fps = 30, codec = 'h264' }
    audio_default_codec = { channel = 2, sample_rate = 44100 }
    if value.codec == nil then
        value.codec = { video = video_default_codec, audio = audio_default_codec }
    else
        if value.codec.audio ~= nil then
            if value.codec.audio.channel == nil then
                value.codec.audio.channel = 2
            end

            if value.codec.audio.sample_rate == nil then
                value.codec.audio.sample_rate = 44100
            end
        end

        if value.codec.video ~= nil then
            if value.codec.video.width == nil or value.codec.video.width == nil then
                value.codec.video.width = 1920
                value.codec.video.height = 1080
            end

            if value.codec.video.kbps == nil then
                value.codec.video.kbps = 2000
            end

            if value.codec.video.fps == nil then
                value.codec.video.fps = 30
            end

            if value.codec.video.codec == nil then
                value.codec.video.codec = 'h264'
            end

        end
    end
end

function checkPushArgs(value)
    if value ~= nil and
       value.codec ~= nil and
       value.codec.audio ~= nil and
       value.codec.audio.channel ~= nil then
       if value.codec.audio.channel <= 0 or value.codec.audio.channel >= 4 then
           value.codec.audio.channel = 2
       end
   end
end

function addPushStream(value)
    fillPushDefaultArgs(value)
    checkPushArgs(value)

    name = value.stream_name

    if _G.onPush[name] == nil then
        property = value

        if property.codec.video ~= nil then
            video_codec = property.codec.video
        end

        outStream = {}
        stream_publisher = PublisherWrapper(value.push_type, _G.job_key)
        trace_uid = 1
        stream_publisher:setPushParam(name, name, trace_uid)
        stream_publisher:addTraceInfo(_G.job_key, trace_uid, _G.out_stream_name)

        publish_stream_opt = StreamOpt()
        if value.codec.video == nil then
            publish_stream_opt.m_publishVideo = false
        end
        if value.codec.audio == nil then
            publish_stream_opt.m_publishAudio = false
        end
        if value.codec.other ~= nil and value.codec.other.scriptag ~= nil then
            publish_stream_opt.m_publishDataStream = true
        end
        stream_publisher:setOpt(publish_stream_opt)

        outStream.video_queue = MediaFrameQueue()
        outStream.audio_queue = MediaFrameQueue()
        outStream.name = name
        outStream.state = 1

        outStream.pusher = stream_publisher
        outStream.property = value
        _G.onPush[name] = outStream

        if property.codec.video ~= nil then
            stream_publisher:setVideoCodec(video_codec.width, video_codec.height,
                video_codec.fps,video_codec.kbps, video_codec.codec)
            encoder = Encoder()
            encoder:init(name, outStream.video_queue, stream_publisher:getVideoQueue()) -- TODO maybe err
            encoder:setCodec(video_codec.width, video_codec.height,
                video_codec.fps, video_codec.kbps, video_codec.codec)

            outStream.encoder = encoder
            encoder:start()
            LOG('add out stream' .. name .. ',type:' .. property.push_type .. ',w:'
                .. property.codec.video.width .. ',h:' .. property.codec.video.height)
        end

        if property.codec.audio ~= nil then
            audio_codec = property.codec.audio
            channels = 2
            sample_rate = 44100
            codec = 'aac'

            if audio_codec.channel ~= nil then
                channels = audio_codec.channel
            end

            if audio_codec.sample_rate ~= nil then
                sample_rate = audio_codec.sample_rate
            end

            if audio_codec.codec ~= nil then
                codec = audio_codec.codec
            end

            audioEncoder = AudioEncoder()
            audioEncoder:init(name, outStream.audio_queue, stream_publisher:getAudioQueue())
            audioEncoder:setCodec(channels, sample_rate, 192, codec)
            audioEncoder:start()
            outStream.audioEncoder = audioEncoder
        end

        stream_publisher:start()
        LOG('add out stream' .. name .. 'type:' .. property.push_type)
    end
    LOG("addPushStream end")
end

function addSimpleText(value)
    table.insert(_G.streamlist, value)
end

function addPkBar(value)
	LOG('@ pktrace, out_stream_name:' .. _G.out_stream_name .. ', z_order:' .. value.z_order)
    table.insert(_G.streamlist, value)
end


function fillPullDefaultArgs(value)
    if value.put_rect == nil then
        value.put_rect = { left = 0, top = 0, right = 1920, bottom = 1080 }
        value.crop_rect = { left = 0, top = 0, right = 1920, bottom = 1080 }
    end
end

function addPullStream(value)
    LOG("addPullStream 1")
    fillPullDefaultArgs(value)
    name = value.stream_name
    table.insert(_G.cur_input_streamname_list, name)
    table.insert(_G.streamlist, value)
    LOG('out_stream_name:' .. _G.out_stream_name .. ', add ' .. name .. ' to cur_input_streamname_list')

    if _G.onPull[name] == nil then
        LOG("addPullStream 2")
        subscribeContext = SubscribeContext()
        subscribeContext:subscribeVideoFrame()
        subscribeContext:subscribeAudioFrame()
        subscribeContext:subscribeJobFrame(_G.job_key, name)
        LOG("addPullStream 3")

        inStream = {}
        inStream.subscribeContext = subscribeContext
        inStream.name = name
        inStream.frame = nil
        inStream.getVideo = false
        inStream.isGifloaded = false
        inStream.time_ref = 0
        inStream.subscriberName = _G.out_stream_name
        inStream.jobKey = _G.job_key
        _G.onPull[name] = inStream
        LOG('out_stream_name:' .. _G.out_stream_name .. ', addPullStream name:' .. name )
    end
end

initFunctionTable = {
    av_stream = addPullStream,
    out_stream = addPushStream,
    single_text = addSimpleText,
    pk_bar = addPkBar,
    group_container = addPkBar
}

if _ANIM_ then
    _ANIM_.InitJobFunc(initFunctionTable, _G.streamlist)
end


function jobInit(tables)
    _G.cur_input_streamname_list = {}
    for key, value in pairs(tables) do
        if initFunctionTable[value.type] ~= nil then
            initFunctionTable[value.type](value)
        else
            table.insert(_G.streamlist, value)
        end
    end
end


function updateAudioMixerConfig(tables)
    if _G.onPush[_G.out_stream_name] == nil then
        return
    end

    if _G.onPush[_G.out_stream_name].audio_mixer_wrapper == nil then
        return
    end

    for key, value in pairs(tables) do
        if value.volume ~= nil then
            streamname = value.content
            if onPull[streamname] ~= nil then
                uid = onPull[streamname].uid
                _G.onPush[_G.out_stream_name].audio_mixer_wrapper:getRawPtr():updateVolume(uid, value.volume)
                LOG('out_stream_name:' .. _G.out_stream_name .. ',update input:'
                    .. streamname .. ', uid:' .. uid .. ', volume:' .. value.volume)
            end
        end
    end
end

function checkOutput(output)
    name = output.stream_name
    for key,value in pairs(_G.onPush) do
        if _G.onPush[key] ~= nil and name ~= key then
            _G.out_stream_name = name
            FDLOG('MediaLua', 'job_key:' .. _G.job_key .. ', out stream change:' .. key .. ' -> ' .. name)
            LOG('job_key:' .. _G.job_key .. ', out stream change:' .. key .. ' -> ' .. name .. ', publish again')
            value.pusher:stop()
            value.pusher:join()

            value.pusher:setPushParam(name, name, _G.output_uid)
            value.pusher:removeTraceInfo(_G.job_key, _G.output_uid, key)
            value.pusher:addTraceInfo(_G.job_key, _G.output_uid, _G.out_stream_name)
            value.pusher:setStreamName(_G.out_stream_name)
            value.name = name
            value.pusher:start()

            if value.encoder ~= nil then
                value.encoder:removeTraceInfo(_G.job_key, _G.output_uid, key)
                value.encoder:addTraceInfo(_G.job_key, _G.output_uid, _G.out_stream_name)
                value.encoder:setStreamName(_G.out_stream_name)
            end

            if getAudioMixer(key) ~= nil then
                getAudioMixer(key):removeTraceInfo(_G.job_key, _G.output_uid, key)
                getAudioMixer(key):addTraceInfo(_G.job_key, _G.output_uid, _G.out_stream_name)
                getAudioMixer(key):setStreamName(_G.out_stream_name)
            end

            for k,inStream in pairs(_G.onPull) do
                if inStream.video_queue ~= nil then
                    inStream.video_queue:removeTraceInfo(_G.job_key, _G.output_uid, key)
                    inStream.video_queue:addTraceInfo(_G.job_key, _G.output_uid, _G.out_stream_name)
                end

                if inStream.decoder ~= nil then
                    inStream.decoder:removeTraceInfo(_G.job_key, _G.output_uid, key)
                    inStream.decoder:addTraceInfo(_G.job_key, _G.output_uid, _G.out_stream_name)
                end

                _G.puller:notifyRemoveTraceInfo(k, _G.appid, key, _G.job_key)
                _G.puller:notifyAddTraceInfo(k, _G.appid, _G.out_stream_name, _G.job_key)
            end

            _G.onPush[name] = value
            _G.onPush[key] = nil
        else
            if output ~= nil and
               output.codec ~= nil and
               output.codec.video ~= nil and
               _G.onPush[name] ~= nil and
               _G.onPush[name].property.codec ~= nil and
               _G.onPush[name].property.codec.video ~= nil
            then
                w = _G.onPush[name].property.codec.video.width
                h = _G.onPush[name].property.codec.video.height
                if w ~= output.codec.video.width or h ~= output.codec.video.height then
                    LOG('out_stream_name:' .. _G.out_stream_name .. ',output change! setCodec:'.. output.codec.video.height)
                    _G.onPush[name].encoder:setCodec(output.codec.video.width, output.codec.video.height, output.codec.video.fps,
                                                     output.codec.video.kbps, output.codec.video.codec)
                    _G.onPush[name].property = output
                    LOG('out_stream_name:' .. _G.out_stream_name .. ',w:'
                        .. _G.onPush[name].property.codec.video.width .. ',h:' .. _G.onPush[name].property.codec.video.height)
                end
            end
        end
    end
end


function update(table)

    if table.type == 'audioMixerDump' then
        if getAudioMixer(_G.out_stream_name) ~= nil then
            getAudioMixer(_G.out_stream_name):openDumpFile()
        end
    else
        LogTableWithIndent('', table)
        _G.streamlist = {}
        jobInit(table.input_stream_list)
        fillPushDefaultArgs(table.out_stream)
        checkOutput(table.out_stream)
        updateBgColor(table.out_stream)
    end

end

function stop()

    for key,value in pairs(_G.onPush) do
        LOG('stop push:' .. key)
        value.pusher:stop()
        LOG('join push:' .. key)
        value.pusher:join()

        if value.encoder ~= nil then
            LOG('stop encoder:' .. key)
            value.encoder:stop()
            LOG('join encoder:' .. key)
            value.encoder:join()
        end

        if value.audioEncoder ~= nil then
            LOG('stop audio encoder:' .. key)
            value.audioEncoder:stop()
            LOG('join audio encoder:' .. key)
            value.audioEncoder:join()
        end

    end
    LOG('all stop')
end

function http_url_update()
    local newOnDown = {}
    for key, value in pairs(_G.onDown) do
        path = _G.netUtil:getImagePath(value.content)
        if path  ~= '' then
            if _G.imagelist[value.content] == nil or
               _G.imagelist[value.content].bin == nil
            then
                LOG('download ' .. value.content .. ' ok')
                logo = MediaFrame()
                _G.painter:loadLogo(path, logo)
                value.frame = logo
                if _G.imagelist[value.content] == nil then
                    _G.imagelist[value.content] = {}
                end
                _G.imagelist[value.content].bin = logo
                table.insert(_G.streamlist, value)
            end
        else
            newOnDown[key] = value
        end
    end
    _G.onDown = newOnDown
end

function process()
    update_ms()

    for k, v in pairs(_G.onPush) do
        if _G.onPush[k].property.codec.audio ~= nil then
            onAudioMix(k)
        end
        if _G.onPush[k].property.codec.video ~= nil then
            onVideoMix(k)
        end
    end

    if now_ms() - _G.state_trace_time_ms >= 1000 then
        _G.state_trace_time_ms = now_ms()
        LOG('memory use:' .. collectgarbage("count") .. ' KB')
    end
end


function onAudioMix(name)
    if _G.audioMixer == nil then
        _G.audioMixer = FFmpegAudioMixer()
    end

    frame_ms = 44100.0/2048.0

    if _G.onPush[name].property.codec.audio.codec == 'opus' then
        frame_ms = 10
    end

    if _G.mix_audio_tick == 0 then
        _G.mix_audio_tick = math.floor(now_ms() / frame_ms)
        return
    end

    tick = now_ms() / frame_ms

    if tick - _G.mix_audio_tick < 1.00 then
        return
    end

    if debug_flag then
        diff = now_ms() - _G.pre_mix_audio_ms
        LOG('out_stream_name:' .. _G.out_stream_name .. ', get mixed audio diff:' .. diff)
    end

    _G.pre_mix_audio_ms = now_ms()

    _G.mix_audio_tick = math.floor(tick)

    dst_frame = nil
    for key, value in pairs(_G.onPull) do
        audio_frame = MediaFrame()
        audio_queue = value.subscribeContext:getAudioFrameQueue()
        if audio_queue ~= nil then
            if audio_queue:pop(audio_frame, 0) then
                onPull[key].time_ref = audio_frame:getDts()
                if dst_frame == nil then
                    dst_frame = audio_frame
                else
                    _G.audio_frame_id = _G.audio_frame_id + 1
                    _G.audioMixer:mixFrame(audio_frame, dst_frame)
                end
            end
        end
    end

    if dst_frame ~= nil then
        onPush[name].audio_queue:push(now_ms(), dst_frame)
    end
end

function collectFrame()
    for key, value in pairs(_G.onPull) do
        video_small_frame = MediaFrame()
        video_queue = value.subscribeContext:getVideoFrameQueue()
        if video_queue ~= nil then
            if video_queue:pop_by_given_time_ref(value.time_ref, video_small_frame) then
                value.frame = video_small_frame
                value.getVideo = true
            end
        end
    end
end

function drawImage(frame, image, x, y, w, h, cropRect, clipPolygon)
  local draw_ = function(frame, image, x, y, w, h, cropRect, clipPolygon)
    if (cropRect == nil and clipPolygon == nil) then
      _G.painter:addYUV(frame, image, Point(x, y), w, h)
    else
      cropRect_ = cropRect or {left=0, top=0, right=image:getWidth(), bottom=image:getHeight()}
      local polygons = {}
      if clipPolygon ~= nil then
        for index, element in pairs(clipPolygon) do
          polygons[index] = Point(element.x, element.y)
        end
      end
      clipFactor = 2
      LOG('addYUV with clip polygon begin')
      _G.painter:addYUV(frame, image,  Point(x, y), w, h,
        Rect(cropRect_.left, cropRect_.top, cropRect_.right - cropRect_.left, cropRect_.bottom - cropRect_.top),
        polygons, clipFactor)
      LOG('addYUV with clip polygon end')
      end
  end

  draw_(frame, image, x, y, w, h, cropRect, clipPolygon)
end

function mixUrl(video_frame, value)
  if _G.imagelist[value.content] ~= nil and
     (_G.imagelist[value.content].bin ~= nil or
     _G.imagelist[value.content].default ~= nil)
  then
      url_image_bin = _G.imagelist[value.content].bin or _G.imagelist[value.content].default

      if value.clip_circle ~= nil and
      (_G.imagelist[value.content].clip_circle == nil or _G.imagelist[value.content].clip_circle ~= value.clip_circle) then
          if value.clip_circle == true then
              _G.painter:addCircleAlphaCustom(url_image_bin,
                  Point(url_image_bin:getWidth() / 2, url_image_bin:getWidth() / 2),
                  value.put_rect.right - value.put_rect.left);
          end
      end

      x = value.put_rect.left
      y = value.put_rect.top
      w = value.put_rect.right - value.put_rect.left
      h = value.put_rect.bottom - value.put_rect.top

      if value.clip_circle ~= nil then
        r, g, b, alpha = str2color(value.border_color)
          _G.painter:addCircleYUV(video_frame, Scalar(b, g, r, 0), Point(x + w/2-1, y + h/2-1), w / 2, -1, 16)
      end

    drawImage(video_frame, url_image_bin, x, y, w, h, value.crop_rect, value.clip_polygon)
  end
end

function str2color(color)
    if type(color) == 'string' and string.len(color) >= 7 then
        local r = tonumber(string.sub(color, 2, 3), 16)
        local g = tonumber(string.sub(color, 4, 5), 16)
        local b = tonumber(string.sub(color, 6, 7), 16)
        local alpha = 1.0
        if string.len(color) == 9 then
            alpha = tonumber(string.sub(color, 8, 9), 16)
            alpha = alpha / 255.0
        end
        return r, g, b, alpha
    else
        LOG('parse color error!color:' .. color)
        return 0, 0, 0, 1.0
    end
end

function byteCount(curByte)
    local byteCnt = 1
    if curByte >=192 and curByte < 223 then
        byteCnt = 2
    elseif curByte >= 224 and curByte < 239 then
        byteCnt = 3
    elseif curByte >=240 and curByte <= 247 then
        byteCnt = 4
    end
    return byteCnt
end

function countString(str)
    local width = 0
    local byteLen = #str
    local index = 1
    while index <= byteLen do
        local byteCnt = byteCount( string.byte(str, index) )
        local char = string.sub(str, index, index + byteCnt -1)
        index = index + byteCnt
        width = width +1
    end
    return width
end

function substr(str, i, j)
    if i < 1 or i > j or j > countString(str) then
        return str
    end
    local len = j - i
    byte_index_of_i = 1
    while i > 1 do
        local byteCnt = byteCount( string.byte(str, byte_index_of_i) )
        byte_index_of_i = byte_index_of_i + byteCnt
        i = i-1
    end
    byte_index_of_j = byte_index_of_i
    local len = j - i + 1
    while len > 0 do
        local byteCnt = byteCount( string.byte(str, byte_index_of_j))
        byte_index_of_j = byte_index_of_j + byteCnt
        len = len-1
    end
    return string.sub(str, byte_index_of_i, byte_index_of_j - 1)
end

function formatText(text, size, width, font, leanType, leanAngle)
  word_num = countString(text)
  done = false
  local content_size = Size()
  _G.painter:getWordSize(text, content_size, font, size, leanType, leanAngle)
  while content_size.width > width and word_num > 1 do
      word_num = word_num - 1
      text = substr(text, 1, word_num)
      _G.painter:getWordSize(text .. '...', content_size, font, size, leanType, leanAngle)
      done = true
  end
  if done then
      return text .. '...'
  else
      return text
  end
end

function drawRect(video_frame, point, w, h)
    color = Scalar(0, 0, 0, 0)
    left_bottom = point
    left_top = Point(point.x, point.y-h)
    right_bottom = Point(point.x+w, point.y)
    right_top = Point(point.x+w, point.y-h)
    _G.painter:addLine(video_frame, left_bottom, right_bottom, color, 1)
    _G.painter:addLine(video_frame, left_bottom, left_top, color, 1)
    _G.painter:addLine(video_frame, right_top, right_bottom, color, 1)
    _G.painter:addLine(video_frame, right_top, left_top, color, 1)
end

function mixText(video_frame, value)
    r, g, b, alpha = str2color(value.property.color)
    font_type = value.property.font_type or 'msyh.ttc'
    size = value.property.size
    content = value.content or '我是一颗小虎牙'
    local leanType = 1
    local leanAngle = value.property.lean_angle or 0

    if value.property.left ~= nil then
        x = value.property.left
        y = value.property.top
    else
        x = value.put_rect.left
        y = value.put_rect.top
        if value.put_rect.right ~= nil and value.put_rect.right ~= -1 then
            content = formatText(content, size, value.put_rect.right - value.put_rect.left, font_type, leanType, leanAngle)
        end
    end

    content_size = Size()
    content_offset = Point()
    if value.property.border_width ~= nil and value.property.border_color ~= nil then
        if value.property.border_version == nil or value.property.border_version == 0 then
            _G.painter:getBorderWordSize(content, content_size, value.property.border_width, font_type, size)
        elseif value.property.border_version == 1 then
            _G.painter:getWordWithBorderSize(content, content_size, content_offset,
            value.property.border_width, Scalar(size, 0.8, 0.1, 0), font_type, leanType, leanAngle)
        end
        LOG('content_size, w:' .. content_size.width .. ', h:' .. content_size.height .. ', content_offset.x'
            .. content_offset.x .. ", content_offset.y:" .. content_offset.y)
    else
        _G.painter:getWordSize(content, content_size, font_type, size, leanType, leanAngle)
    end
    if value.property.text_alignment ~= nil then
        if value.property.text_alignment == 'right' then
            x = value.put_rect.right - content_size.width
        elseif value.property.text_alignment == 'center' then
            mid = value.put_rect.left + (value.put_rect.right - value.put_rect.left)/2
            x = mid - (content_size.width / 2)
            mid = value.put_rect.top + (value.put_rect.bottom - value.put_rect.top)/2
            y = mid - (content_size.height / 2)
        elseif value.property.text_alignment == 'center_vertical' then
            mid = value.put_rect.top + (value.put_rect.bottom - value.put_rect.top)/2
            y = mid - (content_size.height / 2)
        elseif value.property.text_alignment == 'center_horizontal' then
            mid = value.put_rect.left + (value.put_rect.right - value.put_rect.left)/2
            x = mid - (content_size.width / 2)
        end
    end

    if value.property.border_width ~= nil and value.property.border_color ~= nil then
        border_r, border_g, border_b, border_alpha = str2color(value.property.border_color)
        if value.property.border_version == nil or value.property.border_version == 0 then

            local bold = 32

             _G.painter:addBoldWord(video_frame, content, Point(x+1, y), Scalar(border_r, border_g, border_r, 1),
                Scalar(size, 0.8, 0.1, 0), alpha, font_type, bold, leanType, leanAngle)
            _G.painter:addBoldWord(video_frame, content, Point(x-1, y), Scalar(border_r, border_g, border_r, 1),
                Scalar(size, 0.8, 0.1, 0), alpha, font_type, bold, leanType, leanAngle)

            _G.painter:addBoldWord(video_frame, content, Point(x, y+1), Scalar(border_r, border_g, border_r, 1),
                Scalar(size, 0.8, 0.1, 0), alpha, font_type, bold, leanType, leanAngle)
            _G.painter:addBoldWord(video_frame, content, Point(x, y-1), Scalar(border_r, border_g, border_r, 1),
                Scalar(size, 0.8, 0.1, 0), alpha, font_type, bold, leanType, leanAngle)

            _G.painter:addBoldWord(video_frame, content, Point(x+1, y+1), Scalar(border_r, border_g, border_r, 1),
                Scalar(size, 0.8, 0.1, 0), alpha, font_type, bold, leanType, leanAngle)
            _G.painter:addBoldWord(video_frame, content, Point(x-1, y-1), Scalar(border_b, border_g, border_r, 1),
                Scalar(size, 0.8, 0.1, 0), alpha, font_type, bold, leanType, leanAngle)

            _G.painter:addBoldWord(video_frame, content, Point(x, y), Scalar(b, g, r, 1), Scalar(size, 0.8, 0.1, 0),
                alpha, font_type, bold, leanType, leanAngle)
        elseif value.property.border_version == 1 then
            _G.painter:addWordWithBorder(video_frame, content, value.property.border_width, Point(x, y),
                Scalar(b, g, r, 1), Scalar(border_b, border_g, border_r, 1), Scalar(size, 0.8, 0.1, 0), alpha, font_type, leanType, leanAngle)
        end
    else
        _G.painter:addWord(video_frame, content, Point(x, y),
        Scalar(b, g, r, 1), Scalar(size, 0.8, 0.1, 0), alpha, font_type, leanType, leanAngle)
    end
end

function isFileExist(file_name)
    local file = io.open(file_name, "rb")
    if file then file:close() end
    return file ~= nil
end


function mixImage(local_frame, value)
    LOG('image:' .. value.content)
    if _G.imagelist[value.content] == nil or
       _G.imagelist[value.content].bin == nil then
        local image_path = getImageDir() .. '/' .. value.content
        if isFileExist(image_path) then
            LOG('GetFile path:' .. image_path)
            logo = MediaFrame()
            _G.painter:loadLogo(image_path, logo)
            _G.imagelist[value.content] = {}
            _G.imagelist[value.content].bin = logo
        else
            if _G.imagelist[value.content] == nil or
               _G.imagelist[value.content].default == nil then
                LOG('find no image' .. image_path)
                return
            end
        end
    end
    image = _G.imagelist[value.content].bin or _G.imagelist[value.content].default

    x = value.put_rect.left
    y = value.put_rect.top
    w = value.put_rect.right - value.put_rect.left
    h = value.put_rect.bottom - value.put_rect.top
    if value.crop_rect ~= nil then
        _G.painter:addYUV(local_frame, image, Point(x, y), w, h,
        Rect(value.crop_rect.left, value.crop_rect.top, value.crop_rect.right - value.crop_rect.left,
            value.crop_rect.bottom - value.crop_rect.top))
    else
        _G.painter:addYUV(local_frame, image, Point(x, y), w, h)
    end
end

function getDefaultImage(w, h, default_image)
    if default_image == nil then
        LOG('null default_image')
        default_image = 'defaultImg_375_422.png'
        image_375211 = 'defaultImg_375_211.png'
        image_375667 = 'defaultImg_375_667.png'
        local scale = (w*1.0) / (h*1.0)
        if math.abs(scale - sixteen_nine) < 0.1 then
            return image_375211
        elseif math.abs(scale - nine_sixteen) < 0.1 then
            return image_375667
        end
    else
        LOG('no null default_image')
    end
    return default_image
end

function loadUrlImage(image_name)
    LOG('Bg loadUrlImage' .. image_name)
    if isFileExist(image_name) then
        LOG('Bg GetFile path:' .. image_name)
        logo = MediaFrame()
        _G.painter:loadLogo(image_name, logo)
        _G.imagelist[image_name] = {}
        _G.imagelist[image_name].bin = logo
        return logo
    end
    return nil
end

function loadImage(image_name)
    if _G.imagelist[image_name] == nil or
       _G.imagelist[image_name].bin == nil then
        local image_path = getImageDir() .. '/' .. image_name
        if isFileExist(image_path) then
            LOG('GetFile path:' .. image_path)
            logo = MediaFrame()
            _G.painter:loadLogo(image_path, logo)
            _G.imagelist[image_name] = {}
            _G.imagelist[image_name].bin = logo
            return logo
        else
            LOG('find no image' .. image_path)
            return nil
        end
    else
        return _G.imagelist[image_name].bin
    end
end

function mixFrame(video_frame, value)
    if value.mix_type == 0 or value.mix_type == 2 then
        if _G.onPull[value.stream_name] ~= nil and _G.onPull[value.stream_name].frame ~= nil then
            if value.crop_rect ~= nil then
                _G.painter:addYUV(video_frame, _G.onPull[value.stream_name].frame, Point(value.put_rect.left, value.put_rect.top),
                    value.put_rect.right - value.put_rect.left, value.put_rect.bottom - value.put_rect.top, Rect(value.crop_rect.left,
                    value.crop_rect.top, value.crop_rect.right - value.crop_rect.left, value.crop_rect.bottom - value.crop_rect.top))
            else
                _G.painter:addYUV(video_frame, _G.onPull[value.content].frame, Point(value.put_rect.left, value.put_rect.top),
                    value.put_rect.right - value.put_rect.left, value.put_rect.bottom - value.put_rect.top)
            end
        end
    end
end

function RGB2YUV(R, G, B)
    _G.bg_y =  0.299 * R + 0.587 * G + 0.114 * B
    _G.bg_u = -0.169 * R - 0.331 * G + 0.5 * B + 128
    _G.bg_v =  0.5 * R - 0.419 * G - 0.081 * B + 128
end

function YUV2RGB(Y, U, V)
    Y = Y - 16
    U = U - 128
    V = V - 128
    R = 1.164 * Y + 1.596 * V
    G = 1.164 * Y - 0.392 * U - 0.813 * V
    B = 1.164 * Y + 2.017 * U
    return R,G,B
end

function mixPk(video_frame, value)
    LOG('@ pktrace, function mixPk, out_stream_name:' .. _G.out_stream_name)
    if value.input_list == nil or value.output == nil then
        return
    end

    pk_bar_frame = MediaFrame()
    local x = value.output.left
    local y = value.output.top
    local w = value.output.width
    local h = value.output.height
    local scale = value.output.scale
    local scaledWidth = math.floor(w*scale)
    local scaledHeight = math.floor(h*scale)
    _G.painter:createYUV(pk_bar_frame, w, h, 237, 128, 128)
    _G.painter:setAsTempLayer(pk_bar_frame,video_frame, Rect(x, y, scaledWidth, scaledHeight ))

    _G.pk_list = {}
    for index, element in pairs(value.input_list) do
        element.output = value.output
        if _ANIM_ and _ANIM_.InitJob(element, pk_list) then
        else
            if element.type == "image_url" then
                local noInsertBackSprint = false;
                addImageUrl(element, noInsertBackSprint)
            end
            table.insert(_G.pk_list, element)
        end
    end
    table.sort(_G.pk_list, _G.sort_function)
    for index, element in pairs(_G.pk_list) do
        if mixFunctionTable[element.type] ~= nil then
            mixFunctionTable[element.type](pk_bar_frame, element)
        end
    end

    _G.painter:addYUV(video_frame, pk_bar_frame, Point(x, y), scaledWidth, scaledHeight)
end

mixFunctionTable =
{
    image_url = mixUrl,
    av_stream = mixFrame,
    single_text = mixText,
    image_resource = mixImage,
    pk_bar = mixPk,
    group_container = mixPk
}
if _ANIM_ then
    _ANIM_.InitMixFunc(mixFunctionTable)
end

function onVideoMix(name)
    fps = _G.onPush[name].property.codec.video.fps
    _G.push_fps = fps
    frame_ms = 1000.0/fps
    tick = now_ms() / frame_ms

    if _G.mix_video_tick == 0 then
        _G.mix_video_tick = math.floor(now_ms() / frame_ms)
        return
    end

    if tick - _G.mix_video_tick < 1.0 then
        return
    end

    if debug_flag then
        diff = now_ms() - _G.pre_mix_video_ms
        LOG('out_stream_name:' .. _G.out_stream_name .. ', get mixed video diff:' .. diff)
    end

    _G.pre_mix_video_ms = now_ms()
    _G.mix_video_tick = math.floor(tick)

    collectFrame()

    video_frame = MediaFrame()
    w = _G.onPush[name].property.codec.video.width
    h = _G.onPush[name].property.codec.video.height
    _G.painter:createYUV(video_frame, w, h, _G.bg_y, _G.bg_u, _G.bg_v)

    table.sort(_G.streamlist, _G.sort_function)
    for key, value in pairs(_G.streamlist) do
	LOG('@ pktrace, out_stream_name:' .. _G.out_stream_name .. ' z_order: ' .. key .. ' type: '..value.type)
        if mixFunctionTable[value.type] ~= nil then
            mixFunctionTable[value.type](video_frame, value)
        end
    end

    w = _G.onPush[name].property.codec.video.width
    h = _G.onPush[name].property.codec.video.height
    user_logo = {}
    user_logo.content = 'new.png'
    user_logo.z_order = 0
    user_logo.put_rect = {}
    user_logo.put_rect.left = 0
    user_logo.put_rect.top = 0
    user_logo.put_rect.right = 10
    user_logo.put_rect.bottom = 10

    mixImage(video_frame, user_logo)

    _G.dump_file_tick = _G.dump_file_tick + 1
    if dump_file_tick == _G.dump_file_interval then

        dump_file_tick = 0

        dump_jpg_file = _G.out_stream_name .. '_' .. _G.dump_file_index .. '.jpg'
        dump_yuv_file = _G.out_stream_name .. '_' .. _G.dump_file_index .. '.yuv'

        LOG('dump jpg file=' .. dump_jpg_file .. ',dump yuv file=' .. dump_yuv_file)

        _G.dump_file_index = _G.dump_file_index + 1

        _G.painter:yuvToRgbAndDumpFile(dump_jpg_file, video_frame)
        _G.painter:yuvDump(dump_yuv_file, video_frame)

        if _G.dump_file_index == 10 then
            _G.dump_file_index = 0
        end
    end

    video_dts = now_ms()
    _G.pre_out_video_dts = video_dts

    video_frame:setDts(video_dts)
    video_frame:setPts(video_dts)

    if debug_flag then
        LOG('out_stream_name:' .. _G.out_stream_name .. ', video dts:' .. video_dts)
    end

    onPush[name].video_queue:push(video_frame:getDts(), video_frame)
end

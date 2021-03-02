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

-- animation.lua
local class         = require "class"
local AnimType      = "frame_animation"
local AnimGroupType = "group_animation"
local PropertyAnimType = "property_animation"
local ImageDir      = getCWD() .. "/../data/image/"
package.path = ImageDir .. "?.lua;" .. package.path
local function existFile(file_name) 
  local file = io.open(file_name, "r")
  if file then 
    file:close()
    return true 
  end
  return false
end

local function initLOGD()
  local ok, pp = pcall(require, "pprint")
  if ok then
    pp.setup({show_userdata=true})
    return function(table)
      LOG(pp.tostring(table))
    end
  end
  return function(table) end
end
local function LOGI(msg)
  INFO("[animation] "..msg)
end

local LOGD = initLOGD()

local function LOGE(msg)
  ERROR("[animation] "..msg)
end


local function CopyRect(obj, json)
      -- left
    if json.left then
      obj.left = json.left
    end
    -- top
    if json.top then
      obj.top = json.top
    end
    -- right
    if json.right then
      obj.right = json.right
    end
    -- bottom
    if json.bottom then
      obj.bottom = json.bottom
    end
end

local function CopyRectWH(obj, objRect, json)
    -- left
    if json.left then
      objRect.left = json.left
    end
    -- top
    if json.top then
      objRect.top = json.top
    end
    -- right
    if json.right then
      objRect.right = json.right
    else
      objRect.right = objRect.left + obj.width
    end
    -- bottom
    if json.bottom then
      objRect.bottom = json.bottom
    else
      objRect.bottom = objRect.top + obj.height
    end
end
local Anim = class("Anim");
Anim.type = AnimType
function Anim:ctor(index, content)
  self.index = tonumber(index)
  self.content = content
  self.start_time = 0
  self.current_time = 0
  self.z_order = 0
  self.put_rect = {
    left = 0,
    top = 0,
    right = 0,
    bottom = 0
  }
  self.dest_put_rect = {
    left = 0,
    top = 0,
    right = 0,
    bottom = 0
  }
  self.images = {}
  self.done = false

  self.count = 0
  self.one_duration = 0
  self.fps = 0
  self.width = 0
  self.height = 0

end

function Anim:LoadConfig(loadImage)
  local ok, conf = pcall(require, self.content .. "/config")
  if not ok or not conf then
    LOGE("find no animation config. content:" .. self.content .. " index:" ..  tostring(self.index))
    return false
  end
  self.fps = conf.fps or 1
  if self.fps <= 0 then
    self.fps = 1
  end
  self.count = conf.count or 1
  local ii = 0
  for i = 0, self.count-1 do
    local image = loadImage(self.content .. "/" ..  i .. ".png")
    if image then
      ii = ii + 1
      table.insert(self.images, image)
    end
  end
  self.count = ii
  if 0 == self.count then
    LOGE("this animation length is 0. content:" .. self.content .. " index:" .. tostring(self.index))
    return false
  end
  self.one_duration = (1000 / self.fps) * self.count
  self.width = conf.width or 100
  self.height = conf.height or 100
  self.put_rect.right = self.put_rect.left + self.width
  self.put_rect.bottom = self.put_rect.top + self.height
  return true
end


function Anim:isTimeout()
  if (self.duration <= self.current_time - self.start_time)then
    LOGI("content:" .. self.content .. " index:" .. tostring(self.index) .. "is Timeout")
    self.done = true 
  end
  return self.done
end

function Anim:isDone(now_time)
  
  self.start_draw_time = self.start_draw_time or now_time
  if (self.duration <= now_time - self.start_draw_time) then
    LOGI("content:" .. self.content .. " index:" .. tostring(self.index) .. "is done")
    self.done = true 
  end
  return self.done
end

function Anim:Draw(local_frame)
    local now_time = getRunMs32()
  self:isDone(now_time)
  if self.done then
    return
  end
  
  if #self.property_modify < 1 then 
    self.start_modify_time = nil 
    self.end_modify_time = now_time
  else
    self.start_modify_time = self.start_modify_time or now_time
    self.end_modify_time = self.start_modify_time + self.modify_duration
  end
  if self.change then 
    self.start_modify_time =  now_time
    self.end_modify_time = self.start_modify_time + self.modify_duration
  end
  local time_diff = self.end_modify_time - now_time 
  if time_diff <= 0 then 
      CopyRect(self.put_rect, self.dest_put_rect)
    time_diff = 0
  else
    time_diff = self.modify_duration - time_diff
  end
  local put_rect_offset = {}
  put_rect_offset.left = math.floor((self.dest_put_rect.left - self.put_rect.left) * time_diff / self.modify_duration / self.step_size) * self.step_size
  put_rect_offset.top = math.floor((self.dest_put_rect.top - self.put_rect.top) * time_diff / self.modify_duration / self.step_size) * self.step_size
  put_rect_offset.right = math.floor((self.dest_put_rect.right - self.put_rect.right) * time_diff / self.modify_duration / self.step_size) * self.step_size
  put_rect_offset.bottom = math.floor((self.dest_put_rect.bottom - self.put_rect.bottom) * time_diff / self.modify_duration / self.step_size) * self.step_size

  local n = math.floor((now_time - self.start_draw_time) * self.fps / 1000) % self.count + 1
  local x = self.put_rect.left + put_rect_offset.left
  local y = self.put_rect.top + put_rect_offset.top
  local w = self.put_rect.right - self.put_rect.left + put_rect_offset.right - put_rect_offset.left
  local h = self.put_rect.bottom - self.put_rect.top + put_rect_offset.bottom - put_rect_offset.top
  _G.painter:addYUV(local_frame, self.images[n], Point(x, y), w, h)
  return
end
local AnimGroup = class("AnimGroup");
AnimGroup.type = AnimGroupType
function AnimGroup:ctor()
  self.group = {}
  self.z_order = 0
end

function AnimGroup:Empty()
  if 0 == #self.group then
    return true
  end
  return false
end

function AnimGroup:Draw(local_frame)
  if (#self.group < 1) then
    return
  end
  
  local obj = self.group[1]
  obj:Draw(local_frame)
  if obj.done then
    LOGI("content:" .. obj.content .. " index:" ..  tostring(obj.index) .. "is removed from group")
    table.remove(self.group, 1)
  end
  self:ResetZOrder()
end

function AnimGroup:ResetZOrder()
  if (#self.group > 0) then 
    self.z_order = self.group[1].z_order
    self.index = self.group[1].index
  end 
end

local PropertyAnim = class("PropertyAnim");
PropertyAnim.type = PropertyAnimType
function PropertyAnim:ctor(index, animation_type, content)
  self.index = tonumber(index)
  self.content = content
  self.animation_type = animation_type
  self.start_time = 0
  self.current_time = 0
  self.z_order = 0
  self.duration = 1
  self.step_size = 1
  self.property_modify = ""
  self.put_rect = {
    left = 0,
    top = 0,
    right = 0,
    bottom = 0
  }
  self.dest_put_rect = {
    left = 0,
    top = 0,
    right = 0,
    bottom = 0
  }
  self.dest_crop_rect = {
    left = 0,
    top = 0,
    right = 0,
    bottom = 0
  }
end

function PropertyAnim:Draw(local_frame)
    local now_time = getRunMs32()
  if #self.property_modify < 1 then 
    self.start_modify_time = nil 
    self.end_modify_time = now_time
  else
    self.start_modify_time = self.start_modify_time or now_time
    self.end_modify_time = self.start_modify_time + self.duration
  end
  if self.change then 
    self.start_modify_time =  now_time
    self.end_modify_time = self.start_modify_time + self.duration
  end
  local time_diff = self.end_modify_time - now_time 
  if time_diff <= 0 then 
    CopyRect(self.put_rect, self.dest_put_rect)
    if self.crop_rect ~= nil then 
      CopyRect(self.crop_rect, self.dest_crop_rect)
    end
    time_diff = 0
  else
    time_diff = self.duration - time_diff
  end
  local put_rect_offset = {}

  
  put_rect_offset.left = math.floor((self.dest_put_rect.left - self.put_rect.left) * time_diff / self.duration / self.step_size) * self.step_size
  put_rect_offset.top = math.floor((self.dest_put_rect.top - self.put_rect.top) * time_diff / self.duration / self.step_size) * self.step_size
  put_rect_offset.right = math.floor((self.dest_put_rect.right - self.put_rect.right) * time_diff / self.duration / self.step_size) * self.step_size
  put_rect_offset.bottom = math.floor((self.dest_put_rect.bottom - self.put_rect.bottom) * time_diff / self.duration / self.step_size) * self.step_size

  local x = self.put_rect.left + put_rect_offset.left
  local y = self.put_rect.top + put_rect_offset.top
  local w = self.put_rect.right - self.put_rect.left + put_rect_offset.right - put_rect_offset.left
  local h = self.put_rect.bottom - self.put_rect.top + put_rect_offset.bottom - put_rect_offset.top

  if self.crop_rect ~= nil then 
    local crop_rect_offset = {}
    crop_rect_offset.left = math.floor((self.dest_crop_rect.left - self.crop_rect.left) * time_diff / self.duration / self.step_size) * self.step_size
    crop_rect_offset.top = math.floor((self.dest_crop_rect.top - self.crop_rect.top) * time_diff / self.duration / self.step_size) * self.step_size
    crop_rect_offset.right = math.floor((self.dest_crop_rect.right - self.crop_rect.right) * time_diff / self.duration / self.step_size) * self.step_size
    crop_rect_offset.bottom = math.floor((self.dest_crop_rect.bottom - self.crop_rect.bottom) * time_diff / self.duration / self.step_size) * self.step_size

    _G.painter:addYUV(local_frame, self.image, Point(x, y), w, h, 
    Rect(self.crop_rect.left + crop_rect_offset.left, 
    self.crop_rect.top + crop_rect_offset.top, 
    self.crop_rect.right - self.crop_rect.left + crop_rect_offset.right -  crop_rect_offset.left, 
    self.crop_rect.bottom - self.crop_rect.top + crop_rect_offset.bottom - crop_rect_offset.top))
  else
    _G.painter:addYUV(local_frame, self.image, Point(x, y), w, h)
  end
  return
end
local M = {}
M.ImagePool = {}
function M.LoadImage(path)
  local image = M.ImagePool[path]
  if nil == image then
    local image_path = ImageDir .. path
    if existFile(image_path) then
      image = MediaFrame()
      _G.painter:loadLogo(image_path, image)
      M.ImagePool[path] = image
    else
      LOGE('find no image' .. image_path)
    end
  end
  return image
end
M.AnimPool = {}
function M.GetOrCreateAnim(json)
  if nil == json.index then
    LOGE("find no index")
    return nil
  end
  local ii = tostring(json.index)
  local obj = M.AnimPool[ii]
  if nil == obj then
    if nil == json.content or 0 == #json.content then
      LOGE("find no content. index:" .. tostring(json.index))
      return nil
    end
    if nil == json.start_time then
      LOGE("find no start_time. content:" .. json.content .. " index:" .. tostring(json.index) )
      return nil
    end
    obj = Anim.new(json.index, json.content)
    obj.start_time  = json.start_time
    obj:LoadConfig(M.LoadImage)
    obj.repeat_count = json.repeat_count or 1
    obj.duration = obj.repeat_count * obj.one_duration
    M.AnimPool[ii] = obj
    obj.version = -1
    if json.start_value then
      if json.start_value.put_rect then
        CopyRectWH(obj, obj.put_rect, json.start_value.put_rect)
        CopyRect(obj.dest_put_rect, obj.put_rect)
      end
    end
  end
  if obj.type ~= AnimType then
    LOGE("object original type is " .. AnimType .. ",now is " .. obj.type .. ". content:" .. json.content .. " index:" .. tostring(json.index))
    return nil
  end
  obj.current_time = json.current_time
  obj.z_order = json.z_order or 0
  obj.property_modify = json.property_modify or ""
  obj.modify_duration = json.modify_duration or obj.modify_duration or 7000
  obj.step_size = json.step_size or obj.step_size or 1
  if obj.step_size <= 0 then
    obj.step_size = 1
  end
  if json.version and json.version ~= obj.version then
    obj.version = json.version
    obj.change = 1
  else
    obj.change = nil
  end
  if #obj.property_modify > 0 then
    if json.put_rect then 
      CopyRectWH(obj, obj.dest_put_rect, json.put_rect)
    end --if json.put_rect then 
  else -- if #obj.property_modify ~= 0 then
    if json.put_rect then
      CopyRectWH(obj, obj.put_rect, json.put_rect)
      CopyRect(obj.dest_put_rect, obj.put_rect)
    end -- if json.put_rect then
  end -- if #obj.property_modify ~= 0 then
  if json.output ~= nil then
    obj.output = json.output
  end
  return obj
end


function M.GetOrCreatePropertyAnim(json)
  if nil == json.index then
    LOGE("find no index")
    return nil
  end
  local ii = tostring(json.index)
  local obj = M.AnimPool[ii]
  if nil == obj then
    if nil == json.content or 0 == #json.content then
      LOGE("find no content. index:" .. tostring(json.index))
      return nil
    end
    if nil == json.animation_type or 0 == #json.animation_type then
      LOGE("find no animation_type. content:" .. json.content .. " index:" .. tostring(json.index) )
      return nil
    end
    if nil == json.start_time then
      LOGE("find no start_time. content:" .. json.content .. " index:" .. tostring(json.index) )
      return nil
    end
    obj = PropertyAnim.new(json.index, json.animation_type, json.content)
    obj.start_time = json.start_time
    M.AnimPool[ii] = obj
    obj.image = M.LoadImage(json.content)
    obj.version = -1
    if json.start_value then
      if json.start_value.put_rect then
        CopyRect(obj.put_rect, json.start_value.put_rect)
        CopyRect(obj.dest_put_rect, json.start_value.put_rect)
      end
    end
    if json.start_value then
      if json.start_value.crop_rect then
        obj.crop_rect = {}
        CopyRect(obj.crop_rect, json.start_value.crop_rect)
        CopyRect(obj.dest_crop_rect, json.start_value.crop_rect)
      end
    end
  end
  if obj.type ~= PropertyAnimType then
    LOGE("object original type is " .. PropertyAnimType .. ",now is " .. obj.type .. ". content:" .. json.content .. " index:" .. tostring(json.index))
    return nil
  end
  obj.current_time = json.current_time
  obj.z_order = json.z_order or obj.z_order or 0
  obj.duration = json.duration or obj.duration or 700
  obj.step_size = json.step_size or obj.step_size or 1
  if obj.step_size <= 0 then
    obj.step_size = 1
  end
  obj.property_modify = json.property_modify or ""
  if json.version and json.version ~= obj.version then
    obj.version = json.version
    obj.change = 1
  else
    obj.change = nil
  end
  if #obj.property_modify > 0 then
    if json.put_rect then 
      CopyRect(obj.dest_put_rect, json.put_rect)
    end --if json.put_rect then 
  else -- if #obj.property_modify ~= 0 then
    if json.put_rect then
      CopyRect(obj.put_rect, json.put_rect)
      CopyRect(obj.dest_put_rect, obj.put_rect)
    end -- if json.put_rect then
  end -- if #obj.property_modify ~= 0 then
  
  if #obj.property_modify > 0 then
    if json.crop_rect then 
      CopyRect(obj.dest_crop_rect, json.crop_rect)
    end --if json.put_rect then 
  else -- if #obj.property_modify ~= 0 then
    if json.crop_rect then
      obj.crop_rect = {}
      CopyRect(obj.crop_rect, json.crop_rect)
      CopyRect(obj.dest_crop_rect, obj.crop_rect)
    end -- if json.put_rect then
  end -- if #obj.property_modify ~= 0 then

  if json.output ~= nil then
    obj.output = json.output
  end
  return obj
end

function M.InitJobFunc(funcTable)
  funcTable[AnimType] = function(json)
    local obj = M.GetOrCreateAnim(json)
    if obj then
      table.insert(_G.streamlist, obj) 
    end
  end -- func
  funcTable[AnimGroupType] = function(json)
    if nil == json.group then
      return
    end
    local group = AnimGroup.new()
    for _, jj in ipairs(json.group) do
      if jj.type == nil then
        LOGE("AnimGroup.group.obj.type is nil")
        return 
      end
      if jj.type == AnimType then
        local obj = M.GetOrCreateAnim(jj)
        if obj and not obj:isTimeout() then 
          table.insert(group.group, obj)  
        end
      end
      if jj.type == PropertyAnimType then
        local obj = M.GetOrCreatePropertyAnim(jj)
        if obj then 
          table.insert(group.group, obj)  
        end
      end
    end -- for
    if not group:Empty() then
      table.sort(group.group, 
        function(left, right)
          if left.index < right.index then
            return true
          end      

          return false
        end)
      group:ResetZOrder()
      table.insert(_G.streamlist, group) 
    end
  end -- func
  funcTable[PropertyAnimType] = function(json)
  local obj = M.GetOrCreatePropertyAnim(json)
  if obj then
      table.insert(_G.streamlist, obj) 
    end
  end -- func
end

function M.InitJob(value, list)
  if value.type and value.type == PropertyAnimType then
    local obj = M.GetOrCreatePropertyAnim(value)
    if obj then
      table.insert(list, obj) 
      return true
    end
  end
  if value.type and value.type == AnimType then
    local obj = M.GetOrCreateAnim(value)
    if obj then
      table.insert(list, obj) 
      return true
    end
  end
  
  if value.type and value.type == AnimGroupType then
    if nil == value.group then
      return true
    end
    local group = AnimGroup.new()
    for _, jj in ipairs(value.group) do
      local obj = M.GetOrCreate(jj)
      if obj and not obj:isTimeout() then 
        table.insert(group.group, obj)  
      end
    end -- for
    if not group:Empty() then
      table.sort(group.group, 
        function(left, right)
          if left.index < right.index then
            return true
          end      
          return false
        end)
      group:ResetZOrder()
      table.insert(list, group) 
      return true
    end
  end
  
  return false
end


function M.InitMixFunc(funcTable)
  funcTable[AnimType] = function(local_frame, obj)
    obj:Draw(local_frame)
  end
  funcTable[AnimGroupType] = function(local_frame, group)
    group:Draw(local_frame)
  end
  funcTable[PropertyAnimType] = function(local_frame, obj)
    obj:Draw(local_frame)
  end
end

function M.sortComp(left, right)
  if left == nil or left.z_order == nil then
      return false
  end
  if right == nil or right.z_order == nil then
      return true
  end
  local l, r = tonumber(left.z_order), tonumber(right.z_order)
  if l < r then
    return true
  elseif l > r then
    return false
  end
  if nil == left.index then
    return false
  end
  if nil == right.index then
      return true
  end
  if left.index < right.index then
    return true
  end
  return false
end

return M

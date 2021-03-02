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
function class(classname, super)  
  local cls = {}
  if super then
    cls = {}
    for k,v in pairs(super) do 
      cls[k] = v 
    end
    cls.super = super
  else
    cls = {ctor = function() end}
  end
  --
  cls.__cname = classname
  cls.__index = cls
  --
  function cls.new(...)
    local instance = setmetatable({}, cls)
    local create
    create = function(c, ...)
      if c.super then -- 递归向上调用create
        create(c.super, ...)
      end
      if c.ctor then
        c.ctor(instance, ...)
      end
    end
    create(instance, ...)
    instance.__class = cls
    --
    return instance
  end
  --
  function cls.__call(...)
    print("__call")
    return cls.new(...)
  end
  --
  return cls
end
--
return class

local _INDEX = {}

function _INDEX:setHeader(k, v)
    if type(self.values.header) ~= "table" then
        error("ignore")
    end
	self.values.header[k] = v
	return self
end

function _INDEX:header(v)
    self.values.header = v
    return self
end

function _INDEX:setPostField(k, v)
    if self.values.data == nil then
        self.values.data = {}
    end
    self.values.data[k] = v
    return self
end

function _INDEX:post(v)
	self.values.data = v
	return self
end

function _INDEX:extra(v)
	self.values.extra = v
	return self
end

function _INDEX:commit()
    local req_id = self.stage .. ":";

    if self.callbacks ~= nil then

        req_id =  req_id .. stage.waitfor(function (callback, result)
			callback(result)
            stage.waitfor(_WAITFOR[0], nil)
        end, self.callbacks)
    end

    return stage.signal(req_id, self.values)
end
--
--
local CURL = {}
function CURL.New(stage_id, url, callback)
    if index == nil then
        index = "curl"
    end
    if callback == nil then
        callback = {}
    end
    return setmetatable({
        stage = stage_id, values = { url = url, header = {} }, callbacks = callback
    }, { __index = _INDEX })
end
return CURL

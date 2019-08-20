
local function JOIN()
	if #_CLUSTER_READY > 0 then
		local entry = _CLUSTER_READY
		
		_CLUSTER_READY = {}
		for k, v in pairs(entry) do
			local p = broker.join(v, function (socket)
				logger.info("CLUSTER - JOIN", broker.ntoa(socket[1]))
				_CLUSTER_ACCEPT[socket.id] = {}
			end)

			p._join = v;

			p.expire = -1;
			p.commit = 10

			p.receive = stage.load("cluster.receive")
			p.close = function (socket)
				logger.info("JOIN CLOSE", socket.id, socket._join)
				if _CLUSTER_ACCEPT[socket.id] ~= nil then
					for i, v in ipairs(_CLUSTER_ACCEPT[socket.id]) do
						_CLUSTER_STAGE[v][socket.id] = nil
					end
					_CLUSTER_ACCEPT[socket.id] = nil
				end
				_CLUSTER_READY[#_CLUSTER_READY + 1] = socket._join
			end
		end
	end
end

--
--
local function READY(cluster_addr)
	local p = broker.ready(cluster_addr, function (socket, addr)
		logger.info("CLUSTER - ACCEPT", broker.ntoa(addr))
		_CLUSTER_ACCEPT[socket.id] = {}
		return stage.load("cluster.receive")
	end)

	p.expire = -1 
	p.commit = 10
	p.close = function (socket)
		for i, v in ipairs(_CLUSTER_ACCEPT[socket.id]) do
			_CLUSTER_STAGE[v][socket.id] = nil
		end
		_CLUSTER_ACCEPT[socket.id] = nil
		logger.info("CLUSTER - CLOSE", socket.id)
	end
	return p
end

--
--
-- stage.load("cluster.ready", function (cluster)
-- 	local addr = process["$%cluster"]
-- 	if addr ~= nil then
-- 		return cluster(addr)
-- 	end
-- end)
--[[
stage.waitfor("callback", function (v)
	print("WAITFOR-CALLBACK", v)
end)

stage.addtask(1, function ()
	local cluster = stage.load("cluster.ready")()
	cluster:signal("test+hello=callback", { math.random(), math.random() })
end)
]]--
return function (cluster_addr)
	if cluster_addr == nil  then
	else
		if _CLUSTER_ACCEPT == nil then
			_SANDBOX[""] = { "*_CLUSTER_ACCEPT", "*_CLUSTER_STAGE" }
			_CLUSTER_ACCEPT = {}
			_CLUSTER_STAGE = {}
		end

		if type(cluster_addr) == "table" then
			if _CLUSTER_READY ~= nil then
				for k, v in ipairs(cluster_addr) do
					_CLUSTER_READY[#_CLUSTER_READY + 1] = v
				end
			else
				_SANDBOX[""] = { "*_CLUSTER_READY" }

				_CLUSTER_READY = cluster_addr
				stage.addtask(1000, JOIN)
			end
		else
			READY(cluster_addr)
		end
	end

	local cluster = stage.proxy({}, {
		"cluster",
		function (k, i)
			return k
		end,
		[""] = function (t, o, k, v)
			-- print("COMMIT", k, v)
			if type(v) == "function" then
				return true
			end
			return false
		end
	})

	-- cluster:signal("stage:command=callback", value)
	function cluster:signal(k, v)
		local i = k:split("=")
		local s = i[1]:split(":")

		local stage
		if #s > 2 then
			stage = _CLUSTER_STAGE[s[1]]
			if stage == nil then
				return 0
			end
		else
			stage = _CLUSTER_ACCEPT
		end

		local r = {}
		local notify = 0
		r[ s[#s] ] = { (#i > 1) and i[2] or "", v }
		for f, i in pairs(stage) do
			local socket = broker.f(f)
			if (socket ~= nil) and (socket.commit(r) > 0) then
				notify = notify + 1
			end
		end
		-- print("SELF", self, k, v)
		return notify
	end
	return cluster
end

const BSON = require('bson')

function getNetworkInterfaceIP(device) {
	if (require('net').isIPv4(device))
		return device;
	else {
		var interfaces = require('os').networkInterfaces();
		for (var dev in interfaces)
			if ((device == null) || (dev.indexOf(device) >= 0))
			{
				var iface = interfaces[dev];
				for (var i = 0; i < iface.length; i++)
				{
					var alias = iface[i];
					if (alias.family === 'IPv4' && (alias.internal == false))
						return alias.address;
				}
			}
	}
	return null;	
}

function bytesToHex(bytes) {
	for (var hex = [], i = 0; i < bytes.length; i++) {
			hex.push((bytes[i] >>> 4).toString(16));
			hex.push((bytes[i] & 0xF).toString(16));
	}
	return hex.join("");
}

function Feedback(r__i, i__c) {
	for (var k in r__i) {
    if (r__i.hasOwnProperty(k)) this[k] = r__i[k];
	}	
	this.callback = i__c;
 };

/* signal(args...) */
Feedback.prototype.signal = function (...args) {
	if (this.stage && this.callback && this.port)
	{
		var data = BSON.serialize({
			r: { s: this.sign },
			['$']: { [this.stage + ":" + this.callback]: args }
		});
		return this.io.send(data, 0, data.length, this.port, this.address);
	}
}

function STAGE(sign, server_ip, server_port, device) {
	var _this = this;

	_this.waitfors = {};

	_this.sign = sign;
	_this.io = require('dgram').createSocket('udp4');
	_this.io.on('message', function (data, addr) {
		var r__i = {};
		var r__v = null;

		var json_v = BSON.deserialize(data);
		// console.log("message: " + JSON.stringify(json_v));
		for (k in json_v)
		{
			var v = json_v[k];
			if (k == "%");
			else if (k == "r") for (var r_k in v)
			{
				if (r_k == "s") r__i.stage = v[r_k];
				else if (r_k == "i") v[r_k].forEach(function (v) {
					if (v instanceof BSON.Binary);
					else if (typeof v == "string") r__i.address = v;
					else r__i.port = v;
				});
			}
			else if (k == "v") r__v = v;
			else if (k == "$") Object.keys(v).forEach(function (k) {
				var k__i = k.indexOf(':');
				if ((k__i < 0) && (_this.sign == null));
				else if ((k__i > 0) && _this.sign && (k.substr(0, k__i) === _this.sign))
				{
					var k__p = k.substr(k__i + 1);
					if (_this.waitfors.hasOwnProperty(k__p))
					{
						var x__w = _this.waitfors[k__p];
						var x__v = (typeof(v[k]) == "object") ? new Proxy(v[k], {
							get(target, property) {
								if (typeof(property) == "string")
									property = ((parseInt(property).toString() == property) ? "#": "") + property;
								return target[property];
							}
						}): v[k];	
						x__w[0].apply({}, x__w.slice(1).concat(x__v));
					}
				}
			});
		}

		if (r__v) Object.keys(r__v).forEach(function (k) {
			var k__i = k.indexOf(':');
			if ((k__i < 0) && (_this.sign == null));
			else if ((k__i > 0) && _this.sign && (k.substr(0, k__i) === _this.sign))
			{
				var k__p = k.substr(k__i + 1).split('=');
				if (_this.waitfors.hasOwnProperty(k__p[0]))
				{
					var x__f = new Feedback(r__i, k__p[k__p.length - 1]);
					var x__w = _this.waitfors[k__p[0]];

					var x__v = (typeof(r__v[k]) == "object") ? new Proxy(r__v[k], {
						get(target, property) {
							if (typeof(property) == "string")
								property = ((parseInt(property).toString() == property) ? "#": "") + property;
							return target[property];
						}
					}): r__v[k];

					x__f.io = _this.io;
					x__f.sign = _this.sign;
					x__w[0].apply(x__f, x__w.slice(1).concat(x__v));
					x__f = null;
				}
			}
		});		
	});

	_this.server = { address: server_ip, port: server_port };
	_this.local = { address: getNetworkInterfaceIP(device ? device: "eth") };
	return new Promise(function (resolve, reject) {
		_this.io.on('listening', function () {
			_this.local.port = _this.io.address().port;
			_this.timer = setInterval(function (_this) {
				var data = BSON.serialize({ "": [ _this.sign ]});
				_this.io.send(data, 0, data.length, _this.server.port, _this.server.address);	
			}, 1500, _this);
		
			// console.log("READY: " + _this.local.address + ":" + _this.local.port);
			resolve(_this);
		});

		_this.io.bind();
	});
}

STAGE.prototype.waitfor = function (wait_id, callback, ...args) {
	if (callback == undefined) 
		return this.waitfors.hasOwnProperty(wait_id);

	this.waitfors[wait_id] = null;
	if (callback)
		this.waitfors[wait_id] = [callback].concat(args);
	return true;
};

/* signal("stage_id:event_id", args...) */
STAGE.prototype.signal = function (event_id, ...args) {
	var stage_id = '=';
	var i = event_id.indexOf(':');

	if (i < 0);
	else if (i > 0) stage_id = event_id.substring(0, i);
	else if (this.sign != null) event_id = this.sign + ":" + event_id.substring(i + 1);
	if (this.sign != null) 

	var O = {
		v: {},
		r: {}
	};
	
	if (this.sign != null)
	{
		O.r.s = this.sign;
		event_id = event_id.replace("\+", this.sign + ":");
	}

	if (this.local.port != 0) O.r.i = [this.local.address, this.local.port];

	O.v[event_id] = args;
	var data = BSON.serialize({[stage_id]: O});
	return this.io.send(data, 0, data.length, this.server.port, this.server.address);
}

/* ------------------example */
new STAGE("nodejs", "10.211.55.14", 9801, "vnic").then(function (stage) {
	console.log("START"); 
	stage.waitfor("result", function (v) {
		console.log("RESULT: ", v[1], v[2])
	});
	stage.waitfor("hello", function (v) {
		console.log("HELLO", v);
		this.signal("BYE", "NODEJS");
	})
	stage.signal("stage:hello=result", "hi", 10);
}).catch(function (e) { console.log("ERROR", e); })

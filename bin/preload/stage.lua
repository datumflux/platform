broker = require('broker')
logger = require('logger')
odbc = require('odbc')

bundle = require('bundle')
for k, v in pairs(bundle) do
	stage[k] = v
end

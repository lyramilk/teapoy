var basedir = "/root/teapoy";
var webdir = "/root/website_teapoy";

require("config.js");

	conf.redis.test_redis = {
		host:"172.17.72.54",
		port:6379,
		password:"cyxOO2cycyxOO2cycyxOO2cycyxOO2cy",
		readonly:false,
		enablelog:true,
	}
	conf.redis.test_ssdb = {
		host:"172.17.72.54",
		port:8888,
		password:"cyxOO2cycyxOO2cycyxOO2cycyxOO2cy",
		readonly:false,
		enablelog:true,
	}
	conf.redis.familyR = {
		host:"172.17.72.54",
		port:8888,
		password:"cyxOO2cycyxOO2cycyxOO2cycyxOO2cy",
		readonly:true,
		enablelog:true,
	}
	conf.redis.familyW = {
		host:"172.17.72.54",
		port:8888,
		password:"cyxOO2cycyxOO2cycyxOO2cycyxOO2cy",
		readonly:false,
		enablelog:true,
	}
	conf.redis.test = {
		host:"172.17.72.54",
		port:6379,
		readonly:false
	}
	conf.redis.yinyuegushi = {
		host:"172.17.72.54",
		port:6379,
	}
	conf.redis.haoshengyin = {
		host:"172.17.72.54",
		port:6379,
	}
	conf.redis.praise = {
		host:"172.17.72.54",
		port:6379,
	}
	conf.redis.publish = {
		host:"172.17.72.54",
		port:6379,
	}
	conf.redis.cache = {
		host:"172.17.72.54",
		port:6379,
	}

	conf.redis.music = {
		host:"60.28.220.101",
		port:46333,
	}

	conf.redis.comment = {
		host:"60.28.204.133",
		port:56380,
	}

	conf.redis.fans = {
		host:"60.28.204.140",
		port:8000,
	}

	conf.redis.user = {
		host:"60.28.204.134",
		port:8889,
	}

set_config(conf);
var self = conf.self;

//	HTTP网站
var srv = new httpserver();
srv.open(self.port);
srv.bind_url(url_mapping);

//	异步IO模型
var epfd = new epoll();
epfd.add(srv);
epfd.active(self.threadscount);

//	切换用户权限
//su("duck");

//	设置包含目录
env("js.require",webdir + "/require");

//	任务
for(var i in self.task){
	var filename = self.task[i].filename;
	var type = self.task[i].type;
	trace("启动[" + type + "]任务" + filename);
	task(type,filename);
}
//create_repeater(8000,"0.0.0.0","127.0.0.1",22,false,30);
epfd.wait();

/*
var master = new teapoy();
*/

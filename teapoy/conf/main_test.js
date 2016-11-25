echo("启动脚本开始");

var basedir = "/root/teapoy";
var webdir = "/root/website_teapoy";

var self = {
	port:80,
	threadscount:8,
}

var url_mapping = [
	{
		"type":"js",
		"pattern" : "^/haoshengyin/(.*)[\\.]js([\\?].*)?$",
		"module" : webdir + "/logic/haoshengyin/${1}.js${2}",
	},
	{
		"type":"js",
		"pattern" : "^/yinyuegushi/(.*)[\\.]js([\\?].*)?$",
		"module" : webdir + "/logic/yinyuegushi/${1}.js${2}",
	},
	{
		"type":"js",
		"pattern" : "^/family/(.*)[\\.]js([\\?].*)?$",
		"module" : webdir + "/logic/family/${1}.js${2}",
	},
	{
		"type":"js",
		"pattern" : "^/test/(.*)[\\.]js([\\?].*)?$",
		"module" : webdir + "/logic/test/${1}.js${2}",
	},
	{
		"type":"js",
		"pattern" : "^/kwcom/(.*)[\\.]js([\\?].*)?$",
		"module" : webdir + "/logic/kwcom/${1}.js${2}",
	},
	{
		"type":"jsx",
		"pattern" : "^(.*)[\\.]jssp([\\?].*)?$",
		"module" : webdir + "/html/${0}",
		"auth":{
			"type":"js",
			"module":webdir + "/auth.js",
		}
	},
	{
		"type":"jsx",
		"pattern" : "^.*[\\.]jsx([\\?].*)?$",
		"module" : webdir + "/html/${0}",
		"auth":{
			"type":"js",
			"module":webdir + "/auth.js",
		}
	},
];

var conf = {
	"self":{
		"port":8210,
		"threadscount":64,
		"debug":false,
		"runas":"duck",
		"logfile":basedir + "/logs/duck.log",
		"logtype":{
			"trace":true,
			"warning":true,
			"debug":false,
			"error":true,
		},
		"plugin":[
			basedir + "/lib64/libserver.so",
		],
		"task":[
			webdir + "/task/haoshengyin/task1.js",
			webdir + "/task/haoshengyin/task2.js",
			webdir + "/task/kwcom/publish.js",
			webdir + "/task/yinyuegushi/task_homepage.js",
			webdir + "/task/yinyuegushi/task_daren.js",
		],
	},
	"pkey":{
		"iws":"qsmyhsgi",
		"kuwo":"ylzsxkwm",
		"kuwo_wm":"ylzsxkwm",
		"kuwosymbian":"rsitmndw",
		"pachira":"pachiraa",
		"kradio":"kradio99",
		"htc":"htcsecret",
		"ar":"kwks&@69",
		"flow":"12345678",
		"ios":"hdljdkwm",
	},
	"uploader":{
		"path":"/data/upload/resource/kstory/work",
		"dbpath":"/data/duck/leveldb/upload",
		"urlprefix":"http://media.cdn.kuwo.cn/resource/kstory/work",
		"urlupload":"http://kstoryupload.kuwo.cn/kwcom/upload.js",
		"port":8211,
	},
	"leveldb":{
		"cache":{
			"path":"/data/duck/leveldb/cache",
		},
	},
	"redis":{
		"test_redis":{	//好声音
			"host":"172.17.72.54",
			"port":6379,
			//"password":"cyxOO2cycyxOO2cycyxOO2cycyxOO2cy"
		},
		"test_ssdb":{	//好声音
			"host":"172.17.72.54",
			"port":8888,
			"password":"cyxOO2cycyxOO2cycyxOO2cycyxOO2cy"
		},
		"haoshengyin":{	//好声音
			"host":"192.168.220.103",
			"port":6001,
			"password":"cyxOO2cy"
		},
		"praise":{	//点赞
			"host":"192.168.220.116",
			"port":6003,
			"password":"cyxOO2cycyxOO2cycyxOO2cycyxOO2cy"
		},
		"publish":{	//朋友圈
			"host":"192.168.220.116",
			"port":6003,
			"password":"cyxOO2cycyxOO2cycyxOO2cycyxOO2cy"
		},
		"yinyuegushi":{	//音乐故事
			"host":"192.168.226.168",
			"port":6005,
			"password":"cyxOO2cy"
		},
		"cache":{	//音乐故事
			"host":"192.168.220.116",
			"port":6007,
			"password":"cyxOO2cy"
		},
		"fans":{	//关注关系（从于192.168.204.140）
			"host":"192.168.220.116",
			"port":6011,
			"password":"cyxOO2cy"
		},
		/*别人的数据库*/
		"music":{	//歌曲信息
			"host":"192.168.220.101",
			"port":46333,
		},
		"comment":{	//评论信息
			"host":"192.168.204.133",
			"port":56380,
		},
		/*
		"fans":{	//关注关系
			"host":"192.168.204.140",
			"port":8000,
		},*/
		"user":{	//用户信息
			"host":"192.168.204.134",
			"port":8889,
		},
	},
	"cache":{
		"db":"/leveldb/cache",
	},
	"mysql":{
		"ksing":{	// K歌
			"cnf":{
				"file":basedir + "/conf/mysql_ksing.cnf",
				"group":"ksing",
			},
			"opt":[
				"set names utf8",
			],
		},
		"yinyuegushi":{	//音乐故事
			"cnf":{
				"file":basedir + "/conf/mysql_yinyuegushi.cnf",
				"group":"release",
			},
			"opt":[
				"set names utf8",
			],
		},
	},
/**/
	"mod":{
		"haoshengyin":{
			"db":"/redis/haoshengyin",
			"tmpdb":"/redis/haoshengyin",
			"prefix":"kw:ksing:match:",
			"op":"view",
		},
		"yinyuegushi":{
			"db":"/redis/yinyuegushi",
			"mysql":"/mysql/yinyuegushi",
			"db_debug":"/redis/yinyuegushi",
			"mysql_debug":"/mysql/yinyuegushi",
			"prefix":"kw:ksing:mcstory:",
			"op":"view",
		},
		"clan":{	//战队
			"db":"/redis/haoshengyin",
			"tmpdb":"/redis/haoshengyin",
			"prefix":"kw:clan:",
		},
		"publish":{	//朋友圈
			"db":"/redis/publish",
			"tmpdb":"/redis/publish",
			"prefix":"kw:publish:",
		},
		"praise":{	//点赞
			"db":"/redis/praise",
			"tmpdb":"/redis/praise",
			"prefix":"kw:praise:",
		},
	},/**/
}

	conf.redis.test = {
		"host":"172.17.72.54",
		"port":6379,
	}
	conf.redis.yinyuegushi = {
		"host":"172.17.72.54",
		"port":6379,
	}
	conf.redis.haoshengyin = {
		"host":"172.17.72.54",
		"port":6379,
	}
	conf.redis.praise = {
		"host":"172.17.72.54",
		"port":6379,
	}
	conf.redis.publish = {
		"host":"172.17.72.54",
		"port":6379,
	}
	conf.redis.cache = {
		"host":"172.17.72.54",
		"port":6379,
	}

	conf.redis.music = {
		"host":"60.28.220.101",
		"port":46333,
	}

	conf.redis.comment = {
		"host":"60.28.204.133",
		"port":56380,
	}

	conf.redis.fans = {
		"host":"60.28.204.140",
		"port":8000,
	}

	conf.redis.user = {
		"host":"60.28.204.134",
		"port":8889,
	}

set_config(conf);

//	HTTP网站
var srv = new httpserver();
srv.open(self.port);
srv.bind_url(url_mapping);
srv.set_root(webdir + "/html");
srv.preload("js",[
	webdir + "/require/db.js"
]);

srv.set_defaultpage([
	"index.jsx",
	"index.html",
	"index.htm",
	"index.js",
	"index.jssp",
]);

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
	var filename = self.task[i];
	trace("启动任务" + filename);
	task(filename);
}
echo("启动脚本结束");
epfd.wait();

/*
var master = new teapoy();
*/

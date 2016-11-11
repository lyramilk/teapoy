echo("启动脚本开始");

//var basedir = "/root/teapoy";
var basedir = "/dev/shm/teapoy";

var self = {
	port:80,
	threadscount:8,
	directory:basedir + "/webapps",
	webcache:"/tmp/html",
}
var url_mapping = [
	{
		"type":"js",
		"pattern" : "^/haoshengyin/(.*)[\\.]js([\\?].*)?$",
		"module" : basedir + "/logic/haoshengyin/${1}.js${2}",
	},
	{
		"type":"js",
		"pattern" : "^/yinyuegushi/(.*)[\\.]js([\\?].*)?$",
		"module" : basedir + "/logic/yinyuegushi/${1}.js${2}",
	},
	{
		"type":"js",
		"pattern" : "^/kwcom/(.*)[\\.]js([\\?].*)?$",
		"module" : basedir + "/logic/kwcom/${1}.js${2}",
	},
	{
		"type":"jsx",
		"pattern" : "^(.*)[\\.]jssp([\\?].*)?$",
		"module" : self.directory + "${0}",
	},
	{
		"type":"jsx",
		"pattern" : "^.*[\\.]jsx([\\?].*)?$",
		"module" : self.directory + "${0}",
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
			basedir + "/logic/haoshengyin/task1.js",
			basedir + "/logic/haoshengyin/task2.js",
			basedir + "/logic/kwcom/tasks/publish.js",
			basedir + "/logic/yinyuegushi/tasks/task_homepage.js",
			basedir + "/logic/yinyuegushi/tasks/task_daren.js",
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
	"hiredis":{
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
	"mod":{
		"haoshengyin":{
			"db":"/hiredis/haoshengyin",
			"tmpdb":"/hiredis/haoshengyin",
			"prefix":"kw:ksing:match:",
			"op":"view",
		},
		"yinyuegushi":{
			"db":"/hiredis/yinyuegushi",
			"mysql":"/mysql/yinyuegushi",
			"db_debug":"/hiredis/yinyuegushi",
			"mysql_debug":"/mysql/yinyuegushi",
			"prefix":"kw:ksing:mcstory:",
			"op":"view",
		},
		"clan":{	//战队
			"db":"/hiredis/haoshengyin",
			"tmpdb":"/hiredis/haoshengyin",
			"prefix":"kw:clan:",
		},
		"publish":{	//朋友圈
			"db":"/hiredis/publish",
			"tmpdb":"/hiredis/publish",
			"prefix":"kw:publish:",
		},
		"praise":{	//点赞
			"db":"/hiredis/praise",
			"tmpdb":"/hiredis/praise",
			"prefix":"kw:praise:",
		},
	},
}

	conf.hiredis.test = {
		"host":"127.0.0.1",
		"port":6001,
	}
	conf.hiredis.yinyuegushi = {
		"host":"127.0.0.1",
		"port":6001,
	}
	conf.hiredis.haoshengyin = {
		"host":"127.0.0.1",
		"port":6001,
	}
	conf.hiredis.praise = {
		"host":"127.0.0.1",
		"port":6001,
	}
	conf.hiredis.publish = {
		"host":"127.0.0.1",
		"port":6001,
	}
	conf.hiredis.cache = {
		"host":"127.0.0.1",
		"port":6001,
	}


	conf.hiredis.music = {
			"host":"60.28.220.101",
			"port":46333,
	}

	conf.hiredis.comment = {
			"host":"60.28.204.133",
			"port":56380,
	}

	conf.hiredis.fans = {
			"host":"60.28.204.140",
			"port":8000,
	}

	conf.hiredis.user = {
			"host":"60.28.204.134",
			"port":8889,
	}

set_config(conf);

//	HTTP网站
var srv = new httpserver();
srv.open(self.port);
srv.bind_url(url_mapping);
srv.set_root(self.directory,self.webcache);
srv.set_index([
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
env("js.require",basedir + "/require");

/*
srv.bind_url("^/haoshengyin/(.*)[\\.]js([\\?].*)?$",basedir + "/logic/haoshengyin/${1}.js${2}");
srv.bind_url("^/yinyuegushi/(.*)[\\.]js([\\?].*)?$",basedir + "/logic/yinyuegushi/${1}.js${2}");
srv.bind_url("^/kwcom/(.*)[\\.]js([\\?].*)?$",basedir + "/logic/kwcom/${1}.js${2}");
srv.bind_url("^.*[\\.]jssp([\\?].*)?$",basedir + "/logic/modules/jssp.js");
srv.bind_url("^.*$",basedir + "/logic/modules/default.js");*/

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

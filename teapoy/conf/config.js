function newid()
{
	if(!this.id) this.id = 0;
	return +this.id;
}


var url_mapping = [
	{
		type:"js",
		method:"GET,POST",
		pattern: "^/haoshengyin/(.*)[\\.]js([\\?].*)?$",
		module: webdir + "/logic/haoshengyin/${1}.js",
	},
	{
		type:"js",
		method:"GET,POST",
		pattern: "^/yinyuegushi/(.*)[\\.]js([\\?].*)?$",
		module: webdir + "/logic/yinyuegushi/${1}.js",
	},
	{
		type:"js",
		method:"GET,POST",
		pattern: "^/family/(.*)[\\.]js([\\?].*)?$",
		module: webdir + "/logic/family/${1}.js",
	},
	{
		type:"js",
		method:"GET,POST",
		pattern: "^/test/(.*)[\\.]js([\\?].*)?$",
		module: webdir + "/logic/test/${1}.js",
	},
	{
		type:"js",
		method:"GET,POST",
		pattern: "^/kwcom/(.*)[\\.]js([\\?].*)?$",
		module: webdir + "/logic/kwcom/${1}.js",
	},
	{
		type:"jsx",
		method:"GET,POST",
		pattern: "^(.*)[\\.]jssp([\\?].*)?$",
		module: webdir + "/html/${0}",
		index:[
			"index.jssp"
		],
		auth:{
			type:"js",
			module:webdir + "/auth.js",
		}
	},
	{
		type:"jsx",
		method:"GET,POST",
		pattern: "^.*[\\.]jsx([\\?].*)?$",
		module: webdir + "/html/${0}",
		index:[
			"index.jsx"
		],
		auth:{
			type:"js",
			module:webdir + "/auth.js",
		}
	},
	{
		type:"static",
		method:"GET,POST",
		pattern: "^.*$",
		module: webdir + "/html/${0}",
		index:[
			"index.html",
			"index.htm",
			"default.html",
			"default.htm",
		],
	},
];

var conf = {
	self:{
		port:8210,
		threadscount:8,
		debug:false,
		runas:"duck",
		logfile:basedir + "/logs/teapoy.log",
		logtype:{
			trace:true,
			warning:true,
			debug:false,
			error:true,
		},
		task:[
				/*
			{type:'js',filename:webdir + "/task/haoshengyin/task1.js"},
			{type:'js',filename:webdir + "/task/haoshengyin/task2.js"},
			{type:'js',filename:webdir + "/task/kwcom/publish.js"},
			{type:'js',filename:webdir + "/task/yinyuegushi/task_homepage.js"},
			{type:'js',filename:webdir + "/task/yinyuegushi/task_daren.js"},*/
			{type:'js',filename:webdir + "/task/test/test.js"},
		],
	},
	pkey:{
		iws:"qsmyhsgi",
		kuwo:"ylzsxkwm",
		kuwo_wm:"ylzsxkwm",
		kuwosymbian:"rsitmndw",
		pachira:"pachiraa",
		kradio:"kradio99",
		htc:"htcsecret",
		ar:"kwks&@69",
		flow:"12345678",
		ios:"hdljdkwm",
	},
	uploader:{
		path:"/data/upload/resource/kstory/work",
		dbpath:"/data/duck/leveldb/upload",
		urlprefix:"http://media.cdn.kuwo.cn/resource/kstory/work",
		urlupload:"http://kstoryupload.kuwo.cn/kwcom/upload.js",
		port:8211,
	},
	leveldb:{
		cache:{
			id:newid(),
			path:"/data/duck/leveldb/cache",
		},
	},
	redis:{
		family:{	//伐木累
			id:newid(),
			host:"192.168.226.168",
			port:6013,
			password:"cyxOO2cycyxOO2cycyxOO2cycyxOO2cy"
		},
		haoshengyin:{	//好声音
			id:newid(),
			host:"192.168.220.103",
			port:6001,
			password:"cyxOO2cycyxOO2cycyxOO2cycyxOO2cy"
		},
		praise:{	//点赞
			id:newid(),
			host:"192.168.220.116",
			port:6003,
			password:"cyxOO2cycyxOO2cycyxOO2cycyxOO2cy"
		},
		publish:{	//朋友圈
			id:newid(),
			host:"192.168.220.116",
			port:6003,
			password:"cyxOO2cycyxOO2cycyxOO2cycyxOO2cy"
		},
		yinyuegushi:{	//音乐故事
			id:newid(),
			host:"192.168.226.168",
			port:6005,
			password:"cyxOO2cycyxOO2cycyxOO2cycyxOO2cy"
		},
		cache:{	//音乐故事
			id:newid(),
			host:"192.168.220.116",
			port:6007,
			password:"cyxOO2cycyxOO2cycyxOO2cycyxOO2cy"
		},
		fans:{	//关注关系（从于192.168.204.140）
			id:newid(),
			host:"192.168.220.116",
			port:6011,
			password:"cyxOO2cycyxOO2cycyxOO2cycyxOO2cy"
		},
		/*别人的数据库*/
		//歌曲信息
		music:{
			id:newid(),
			host:"192.168.220.101",
			port:46333,
			readonly:true,
		},
		//评论信息
		comment:{
			id:newid(),
			host:"192.168.204.133",
			port:56380,
			readonly:true,
		},
		//关注关系
		fans:{
			id:newid(),
			host:"192.168.204.140",
			port:8000,
			readonly:true,
		},
		//用户信息
		user:{
			id:newid(),
			host:"192.168.204.134",
			port:8889,
			readonly:true,
		},
	},
	cache:{
		db:"/leveldb/cache",
	},
	mysql:{
		// K歌
		ksing:{
			id:newid(),
			cnf:{
				file:basedir + "/conf/mysql_ksing.cnf",
				group:"ksing",
			},
			opt:[
				"set names utf8",
			],
		},
		//音乐故事
		yinyuegushi:{
			id:newid(),
			cnf:{
				file:basedir + "/conf/mysql_yinyuegushi.cnf",
				group:"release",
			},
			opt:[
				"set names utf8",
			],
		},
	},
}
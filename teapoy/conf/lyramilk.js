var basedir = "/mnt/xvdb/webroot/www.lyramilk.com";
var webdir = "/mnt/xvdb/webroot/www.lyramilk.com/html";

var url_mapping = [
	{
		type:"jsx",
		method:"GET,POST",
		pattern: "^.*$",
		module: webdir + "${0}",
	},
	{
		type:"static",
		method:"GET,POST",
		pattern: "^.*$",
		module: webdir + "${0}",
		index:[
			"index.html",
			"index.htm",
			"index.jsx",
			"default.html",
			"default.htm",
		],
	},
];

var conf = {
	self:{
		port:8080,
		https_port:443,
		threadscount:8,
		debug:false,
		runas:"duck",
		logfile:basedir + "/logs/duck.log",
		logtype:{
			trace:true,
			warning:true,
			debug:false,
			error:true,
		},
	}
}

conf.self.logtype = {
	trace:true,
	warning:true,
	debug:true,
	error:true,
}

var master = new teapoy();

set_config(conf);
var self = conf.self;
//	日志
if(self.logfile){
	master.set_log_file(self.logfile);
}
if(self.logtype){
	var l = self.logtype;
	for(var k in l){
		master.enable_log(k,l[k]);
	}
}





//	HTTP网站
var srv = new httpserver();
srv.open(self.port);
srv.bind_url(url_mapping);

//	HTTPS网站
var srv_https = new httpserver();
srv_https.open(self.https_port);
srv_https.bind_url(url_mapping);
srv_https.set_ssl("/root/ssl-keygen/other/3_user_www.lyramilk.com.crt","/root/ssl-keygen/other/4_user_www.lyramilk.com.key");
srv_https.set_ssl_verify_locations([
						"/root/ssl-keygen/other/root.crt",
						"/root/ssl-keygen/other/1_cross_Intermediate.crt",
						"/root/ssl-keygen/other/2_issuer_Intermediate.crt",
						"/root/ssl-keygen/other/client/ca.crt",
					]);
srv_https.set_ssl_client_verify(false);

//	异步IO模型
var epfd = new epoll();
epfd.add(srv);
epfd.add(srv_https);
epfd.active(self.threadscount);

//	切换用户权限
//su("duck");

//	设置包含目录
env("js.require",basedir + "/require");

//	任务
for(var i in self.task){
	var filename = self.task[i].filename;
	var type = self.task[i].type;
	trace("启动[" + type + "]任务" + filename);
	task(type,filename);
}



master.noexit(true);

/*
var master = new teapoy();
*/

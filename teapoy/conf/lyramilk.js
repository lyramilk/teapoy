var basedir = "/root/teapoy";
var webdir = "/mnt/xvdb/webroot/www.lyramilk.com";

var url_mapping = [
	{
		type:"jsx",
		pattern: "^(.*)[\\.]jssp([\\?].*)?$",
		module: webdir + "/html/${0}",
		/*auth:{
			type:"js",
			module:webdir + "/auth.js",
		}*/
	},
	{
		type:"jsx",
		pattern: "^.*[\\.]jsx([\\?].*)?$",
		module: webdir + "/html/${0}",
		/*auth:{
			type:"js",
			module:webdir + "/auth.js",
		}*/
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

set_config(conf);
var self = conf.self;

//	HTTP网站
var srv = new httpserver();
srv.open(self.port);
srv.bind_url(url_mapping);
srv.set_root(webdir + "/html");

srv.set_defaultpage([
	"index.jsx",
	"index.html",
	"index.htm",
	"index.js",
	"index.jssp",
]);

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
srv_https.set_root(webdir + "/html");

srv_https.set_defaultpage([
	"index.jsx",
	"index.html",
	"index.htm",
	"index.js",
	"index.jssp",
]);

//	异步IO模型
var epfd = new epoll();
//epfd.add(srv);
epfd.add(srv_https);
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
epfd.wait();

/*
var master = new teapoy();
*/

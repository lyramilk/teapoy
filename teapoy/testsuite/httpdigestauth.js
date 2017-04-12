var digest = "www.lyramilk.com";
var realm = "Welcome to www.lyramilk.com";
var algorithm = "MD5-sess";


function makenonce()
{
	var str = "";
	for(var i=0;i<10;++i){
		str += parseInt(Math.round(Math.random()*100000),16);
	}
	return str;
}

var authtable = {
	"test":"test",
}

function http401(session,response,nc)
{
	nc = nc || 0;
	let nonce = makenonce();
	response.sendError(401);
	response.setHeader('WWW-Authenticate','Digest username="' + digest + '", realm="' + realm + '", nonce="' + nonce + '", algorithm="' + algorithm + '", nc="' + nc + '", qop="auth"');
	session.set("http.digest.nonce",nonce);
	return false;
}

function auth(request,response){
	let session = request.getSession();
	let user = session.get('http.digest.user');
	if(user) return true;

	let authinfo = {};
	{
		let authstr = request.getHeader('Authorization');
		if(!authstr){
			http401(session,response,0);
			return false;
		}
		let authitems = authstr.split(',');
		authitems.forEach(function(item){
			let pos = item.indexOf('=');
			if(pos != -1){
				let k = item.substr(0,pos);
				let v = item.substr(pos+1);
				if(k[0] == '"' || k[0] == "'"){
					k = k.substr(1,k.length - 2);
				}
				if(v[0] == '"' || v[0] == "'"){
					v = v.substr(1,v.length - 2);
				}
				authinfo[k.trim()] = v.trim();
			}
		});

		authinfo.method = request.getMethod();
		authinfo.nonce = session.get('http.digest.nonce');
	}

	if(typeof(authinfo.nonce) == 'string' && authinfo.nonce.length > 0){

		var username = authinfo["Digest username"];
		var password = authtable[username];
		if(!password){
			http401(session,response,authinfo.nc);
			return false;
		}

		var hash = http_digest_authentication(algorithm,authinfo,username,password);
		if(hash == authinfo.response){
			session.set("http.digest.user",username);
			log("trace","以用户名" + username + "验证成功");
			return true;
		}
		log("warning","以用户名" + username + "哈希值" + authinfo.response + "不等于" + hash + "验证失败");
		http401(session,response,authinfo.nc);
		return false;
	}
	http401(session,response,authinfo.nc);
	return false;
}

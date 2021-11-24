#include "script.h"
#include <stdio.h>
#include <libmilk/log.h>
#include <libmilk/dict.h>
#include <libmilk/codes.h>
#include <sys/wait.h>
#include <sys/poll.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

namespace lyramilk{ namespace teapoy{ namespace native
{
	static lyramilk::log::logss log(lyramilk::klog,"teapoy.native");

	lyramilk::data::var jsni_execve(const lyramilk::data::array& args,const lyramilk::data::map& env)
	{

		return true;

		/*
		int fd_in = dup(STDIN_FILENO);
		int fd_out = dup(STDOUT_FILENO);
		int fd_err = dup(STDERR_FILENO);
		


		*/
	}






	class subprocess:public lyramilk::script::sclass
	{
		int proc_fd_in;
		int proc_fd_out;
		int proc_fd_err;
		
		pid_t pid;
	  public:
		static lyramilk::script::sclass* ctr(const lyramilk::data::array& args)
		{
			return new subprocess;
		}
		static void dtr(lyramilk::script::sclass* p)
		{
			delete (subprocess*)p;
		}

		subprocess()
		{
			proc_fd_in = -1;
			proc_fd_out = -1;
			proc_fd_err = -1;
			pid = 0;
		}

		virtual ~subprocess()
		{
			if(proc_fd_in != -1) ::close(proc_fd_in);
			if(proc_fd_out != -1) ::close(proc_fd_out);
			if(proc_fd_err != -1) ::close(proc_fd_err);
			if(pid != 0) waitpid(pid,nullptr,0);
		}

		lyramilk::data::var stdin(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string str_output = args[0].str();
			return ::write(proc_fd_in,str_output.c_str(),str_output.size());
		}

		lyramilk::data::var execve(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string cmd = args[0].str();

			int fd_in[2];
			int fd_out[2];
			int fd_err[2];

			pipe(fd_in);
			pipe(fd_out);
			pipe(fd_err);

			std::vector<std::string> argsitems;
			std::vector<std::string> envitems;

			if(args.size() > 1){
				MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_array);
				const lyramilk::data::array& ar = args[1];
				for(lyramilk::data::array::const_iterator it = ar.begin();it!=ar.end();++it){
					argsitems.push_back(it->str());
				}
			}

			if(args.size() > 2){
				MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,2,lyramilk::data::var::t_array);
				const lyramilk::data::array& ar = args[2];
				for(lyramilk::data::array::const_iterator it = ar.begin();it!=ar.end();++it){
					envitems.push_back(it->str());
				}
			}



			pid = fork();
			if(pid == 0){
				std::vector<char*> argv;
				for(std::vector<std::string>::iterator it = argsitems.begin();it!=argsitems.end();++it){
					argv.push_back((char*)it->c_str());
				}
				argv.push_back(nullptr);

				std::vector<char*> envp;
				for(std::vector<std::string>::iterator it = envitems.begin();it!=envitems.end();++it){
					envp.push_back((char*)it->c_str());
				}
				envp.push_back(nullptr);

				//子进程
				::close(fd_in[1]);
				::close(fd_out[0]);
				::close(fd_err[0]);
				dup2(fd_in[0],STDIN_FILENO);
				dup2(fd_out[1],STDOUT_FILENO);
				dup2(fd_err[1],STDERR_FILENO);

				int r = ::execve(cmd.c_str(),argv.data(),envp.data());
				log(lyramilk::log::error,"execve") << D("启动子进程%s失败:%s",cmd.c_str(),strerror(errno)) << std::endl;
				exit(r);
			}else{
				::close(fd_in[0]);
				::close(fd_out[1]);
				::close(fd_err[1]);
				proc_fd_in = fd_in[1];
				proc_fd_out = fd_out[0];
				proc_fd_err = fd_err[0];
			}
			return (lyramilk::data::uint64)pid;
		}


		lyramilk::data::var wait(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			//static lyramilk::data::coding* coder = lyramilk::data::codes::instance()->getcoder("utf8");


			lyramilk::script::engine* eng = nullptr;
			{
				lyramilk::data::map::const_iterator it = env.find(lyramilk::script::engine::s_env_engine());
				if(it != env.end()){
					lyramilk::data::datawrapper* urd = it->second.userdata();
					if(urd && urd->name() == lyramilk::script::engine_datawrapper::class_name()){
						lyramilk::script::engine_datawrapper* urdp = (lyramilk::script::engine_datawrapper*)urd;
						if(urdp->eng){
							eng = urdp->eng;
						}
					}
				}
			}

			lyramilk::script::objadapter_datawrapper* obj = nullptr;
			{
				lyramilk::data::map::const_iterator it = env.find(lyramilk::script::engine::s_script_object());
				if(it != env.end()){
					lyramilk::data::datawrapper* urd = it->second.userdata();
					if(urd && urd->name() == lyramilk::script::objadapter_datawrapper::class_name()){
						obj = (lyramilk::script::objadapter_datawrapper*)urd;
					}
				}
			}


			if(eng == nullptr){
				log(lyramilk::log::error,"wait") << D("失败:无法获得引擎对象") << std::endl;
			}
			if(obj == nullptr){
				log(lyramilk::log::error,"wait") << D("失败:无法获得脚本内对象") << std::endl;
			}




			std::vector<pollfd> pfds;
			pfds.reserve(2);
			while(true){
				pfds.clear();
				if(proc_fd_out != -1){
					pollfd pfd;
					pfd.fd = proc_fd_out;
					pfd.events = POLLIN;
					pfd.revents = 0;
					pfds.push_back(pfd);
				}

				if(proc_fd_err != -1){
					pollfd pfd;
					pfd.fd = proc_fd_err;
					pfd.events = POLLIN;
					pfd.revents = 0;
					pfds.push_back(pfd);
				}
				if(pfds.size() == 0) break;

				int ret = ::poll(pfds.data(),pfds.size(),6000);
				if(ret > 0){
					for(std::size_t i=0;i<pfds.size();++i){
						pollfd& pfd = pfds[i];

						if(pfd.revents & POLLIN){
							if(pfd.fd == proc_fd_out){
								char buff[4096] = {0};
								int readbytes = read(pfd.fd,buff,sizeof(buff));
								if(readbytes < 0) TODO();

								lyramilk::data::string s(buff,readbytes);

								lyramilk::data::array args;
								args.push_back(lyramilk::data::chunk((const unsigned char*)buff,readbytes));
								lyramilk::data::var ret;
								eng->call_method(obj,"stdout",args,&ret);
							}else if(pfd.fd == proc_fd_err){
								char buff[4096] = {0};
								int readbytes = read(pfd.fd,buff,sizeof(buff));
								if(readbytes < 0) TODO();

								
								lyramilk::data::array args;
								args.push_back(lyramilk::data::chunk((const unsigned char*)buff,readbytes));
								lyramilk::data::var ret;
								eng->call_method(obj,"stderr",args,&ret);
							}
						}
						if(pfd.revents & (POLLERR | POLLHUP | POLLRDHUP)){
							if(pfd.fd == proc_fd_out){
								::close(proc_fd_out);
								proc_fd_out = -1;
							}else if(pfd.fd == proc_fd_err){
								::close(proc_fd_err);
								proc_fd_err = -1;
							}
						}
					}
				}
			}
			waitpid(pid,nullptr,0);
			pid = 0;
			return true;
		}
		lyramilk::data::var stderr(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string str = args[0].str();
			std::cerr << str;
			std::cerr.flush();
			return true;
		}

		lyramilk::data::var stdout(const lyramilk::data::array& args,const lyramilk::data::map& env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string str = args[0].str();
			std::cout << str;
			std::cout.flush();
			return true;
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["stderr"] = lyramilk::script::engine::functional<subprocess,&subprocess::stderr>;
			fn["stdout"] = lyramilk::script::engine::functional<subprocess,&subprocess::stdout>;
			fn["stdin"] = lyramilk::script::engine::functional<subprocess,&subprocess::stdin>;
			fn["execve"] = lyramilk::script::engine::functional<subprocess,&subprocess::execve>;
			fn["wait"] = lyramilk::script::engine::functional<subprocess,&subprocess::wait>;
			p->define("Subprocess",fn,subprocess::ctr,subprocess::dtr);
			return 1;
		}
	};

	static int define(lyramilk::script::engine* p)
	{
		int i = 0;

		i += subprocess::define(p);
		{
			i += subprocess::define(p);
			//p->define("execve",subprocess::define);++i;
		}
		return i;
	}

	static __attribute__ ((constructor)) void __init()
	{
		//signal(SIGCHLD,SIG_IGN);
		lyramilk::teapoy::script_interface_master::instance()->regist("subprocess",define);
	}
}}}
#include "script.h"
#include <libmilk/var.h>
#include <libmilk/log.h>
#include <libmilk/multilanguage.h>
extern "C" {
	#define __STDC_CONSTANT_MACROS
	#include <libavformat/avformat.h>
	#include <libavcodec/avcodec.h>
	#include <libavutil/avutil.h>
	#include <libswscale/swscale.h>

	#include <libavutil/pixfmt.h>
	#include <libavutil/imgutils.h>
}

#include <fstream>
#include <jpeglib.h>
#include <sstream>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h> 
#include <fcntl.h>


namespace lyramilk{ namespace teapoy{ namespace native{
	class Mpeg
	{
		lyramilk::log::logss log;
	  public:
		AVFormatContext *pformat;
	  public:
		static void* ctr(lyramilk::data::var::array args)
		{
			lyramilk::data::string filename = args[0];
			Mpeg * p = new Mpeg(filename);
			if(!p) return nullptr;

			if(avformat_open_input(&p->pformat,filename.c_str(),nullptr,nullptr) < 0){
				delete p;
				throw lyramilk::exception(D("打开文件错误"));
			}
			//只解析格式探测满分的视频
			if(av_format_get_probe_score(p->pformat) != AVPROBE_SCORE_MAX){
				delete p;
				throw lyramilk::exception(D("文件格式错误"));
			}
			return p;
		}
		static void dtr(void* p)
		{
			delete (Mpeg*)p;
		}

		Mpeg(lyramilk::data::string file):log(lyramilk::klog,"teapoy.native.Mpeg")
		{
			pformat = nullptr;
		}

		virtual ~Mpeg()
		{
			if(pformat){
				avformat_close_input(&pformat);
			}
			if(pformat){
				avformat_free_context(pformat);
			}
		}

		lyramilk::data::var todo(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			TODO();
		}

		lyramilk::data::var scan(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			lyramilk::script::engine* e = (lyramilk::script::engine*)env[lyramilk::script::engine::s_env_engine()].userdata(lyramilk::script::engine::s_user_engineptr());
			lyramilk::data::var::array ar;
			ar.push_back(lyramilk::data::var("Mpeg",this));
			return e->createobject("Mpeg.Keyframe",ar);
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["scan"] = lyramilk::script::engine::functional<Mpeg,&Mpeg::scan>;
			p->define("Mpeg",fn,Mpeg::ctr,Mpeg::dtr);
			return 1;
		}
	};

	class Mpeg_KeyFrame
	{
		lyramilk::log::logss log;
		Mpeg* pmaster;
		bool isinit;
		bool isok;

		// ffmpeg
		AVCodecContext  *pcodec;
		AVCodecParameters* pcodecparam;
		AVCodec         *pcc;
		AVFrame *pframe,*pframeRGB;
		SwsContext  *sws;
		AVPacket packet;
		int videoindex;

		//timestamp
		double timestamp;

		// libjpeg
		uint8_t *out_buffer;
		int sws_width, sws_height;
	  public:
		static void* ctr(lyramilk::data::var::array args)
		{
			return new Mpeg_KeyFrame((Mpeg*)args[0].userdata("Mpeg"));
		}
		static void dtr(void* p)
		{
			delete (Mpeg_KeyFrame*)p;
		}

		Mpeg_KeyFrame(Mpeg* pc):log(lyramilk::klog,"teapoy.native.Mpeg.iterator")
		{
			pmaster = pc;
			isok = false;
			isinit = false;
			timestamp = 0;

			out_buffer = nullptr;
			pcodec = nullptr;
			pcodecparam = nullptr;
			pcc = nullptr;
			pframe = nullptr;
			pframeRGB = nullptr;
			sws = nullptr;
			videoindex = -1;
		}

		virtual ~Mpeg_KeyFrame()
		{
			if(sws)sws_freeContext(sws);
			if(out_buffer)av_free(out_buffer);
			if(pframeRGB)av_frame_free(&pframeRGB);
			if(pframe)av_frame_free(&pframe);
			if(pcodec){
				avcodec_close(pcodec);
				avcodec_free_context(&pcodec);
			}
		}

		void init()
		{
			isinit = true;
			AVFormatContext *pformat = pmaster->pformat;
			if(avformat_find_stream_info(pformat,nullptr) <  0){
				lyramilk::data::string msg = D("获取文件流信息错误 %s",pformat->filename);
				log(lyramilk::log::error,__FUNCTION__) << msg << std::endl;
				throw lyramilk::exception(msg);
			}

			for(unsigned int i=0;i<pformat->nb_streams;i++){
				if(pformat->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO){
					videoindex=i;
					break;
				}
			}
			if(videoindex==-1){
				lyramilk::data::string msg = D("无法获取视频流 %s",pformat->filename);
				log(lyramilk::log::error,__FUNCTION__) << msg << std::endl;
				throw lyramilk::exception(msg);
			}

			pcodecparam=pformat->streams[videoindex]->codecpar;
			if(pcodecparam==nullptr){
				lyramilk::data::string msg = D("获取解码器参数失败");
				log(lyramilk::log::error,__FUNCTION__) << msg << std::endl;
				throw lyramilk::exception(msg);
			}
			pcc=avcodec_find_decoder(pcodecparam->codec_id);
			if(pcc==nullptr){
				lyramilk::data::string msg = D("无法找到解码器");
				log(lyramilk::log::error,__FUNCTION__) << msg << std::endl;
				throw lyramilk::exception(msg);
			}
			pcodec = avcodec_alloc_context3(pcc);

			if(avcodec_parameters_to_context(pcodec,pcodecparam) < 0){
				lyramilk::data::string msg = D("获取解码器参数失败");
				log(lyramilk::log::error,__FUNCTION__) << msg << std::endl;
				throw lyramilk::exception(msg);
			}

			if(avcodec_open2(pcodec,pcc,nullptr)<0){
				lyramilk::data::string msg = D("解码器打开文件失败 %s",pmaster->pformat->filename);
				log(lyramilk::log::error,__FUNCTION__) << msg << std::endl;
				throw lyramilk::exception(msg);
			}

			pframe=av_frame_alloc();
			pframeRGB=av_frame_alloc();

			if(pcodec->width > pcodec->height){
				sws_width = 320;
				sws_height = pcodec->height * (double(sws_width) / double(pcodec->width));
			}else{
				sws_height = 240;
				sws_width = pcodec->width * (double(sws_height) / double(pcodec->height));
			}

			out_buffer=(uint8_t *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_RGB24, pcodec->width, pcodec->height,1));
			av_image_fill_arrays(pframeRGB->data,pframeRGB->linesize, out_buffer, AV_PIX_FMT_RGB24, pcodec->width, pcodec->height,1);
			sws = sws_getContext(pcodec->width, pcodec->height, pcodec->pix_fmt,sws_width, sws_height, AV_PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);
		}

		lyramilk::data::var ok(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			if(!isinit){
				init();
				next(args,env);
			}
			return isok;
		}

		lyramilk::data::var save_jpeg(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			lyramilk::data::string jpegdir = args[0].str().c_str();
			if(jpegdir.empty()) throw lyramilk::exception(D("生成JPEG目录不能为空"));

			if(jpegdir[jpegdir.size() - 1] != '/'){
				jpegdir += "/%u.jpg";
			}else{
				jpegdir += "%u.jpg";
			}

			lyramilk::data::string filename;
			{
				int fd = 0;
				char buff[1024];
				srand(::time(0));
				do{
					sprintf(buff,jpegdir.c_str(),(unsigned int)rand());
					fd = open(buff,O_CREAT | O_EXCL,S_IREAD|S_IWRITE|S_IROTH);

					if(fd == -1 && errno != EEXIST){
						throw lyramilk::exception(D("文件错误%s",strerror(errno)));
					}
				}while(fd == -1);
				close(fd);
				filename = buff;
			}

			int quality = 75;
			MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,0,lyramilk::data::var::t_str);
			if(args.size() > 1){
				MILK_CHECK_SCRIPT_ARGS_LOG(log,lyramilk::log::warning,__FUNCTION__,args,1,lyramilk::data::var::t_int32);
				quality = args[1];
			}

			struct jpeg_compress_struct cinfo;
			struct jpeg_error_mgr jerr;
			FILE * outfile;
			int row_stride;
			cinfo.err = jpeg_std_error(&jerr);

			if ((outfile = fopen(filename.c_str(), "wb")) == nullptr) {
				log(lyramilk::log::warning,__FUNCTION__) << D("打开文件%s时发生错误错误：%s",filename.c_str(),strerror(errno)) << std::endl;
				return lyramilk::data::var::nil;
			}

			jpeg_create_compress(&cinfo);
			jpeg_stdio_dest(&cinfo, outfile);
			cinfo.image_width = sws_width;
			cinfo.image_height = sws_height;
			cinfo.input_components = 3;
			cinfo.in_color_space = JCS_RGB;
			jpeg_set_defaults(&cinfo);
			jpeg_set_quality(&cinfo, quality, TRUE);
			jpeg_start_compress(&cinfo, TRUE);
			row_stride = cinfo.image_width * 3;

			JSAMPARRAY image_buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);
			while (cinfo.next_scanline < cinfo.image_height) {
				memcpy(image_buffer[0],pframeRGB->data[0] + cinfo.next_scanline*pframeRGB->linesize[0],row_stride);
				jpeg_write_scanlines(&cinfo, image_buffer, 1);
			}
			jpeg_finish_compress(&cinfo);
			fclose(outfile);
			jpeg_destroy_compress(&cinfo);
			return filename;
		}

		lyramilk::data::var next(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			if(!isinit) init();
			isok = false;
			while(av_read_frame(pmaster->pformat,&packet)>=0)
			{
				if(packet.stream_index==videoindex){
					int ret = 0;
					ret=avcodec_send_packet(pcodec,&packet);
					if(ret < 0){
						if(ret == AVERROR(EAGAIN)) continue;
						lyramilk::data::string msg = D("解码错误%s","avcodec_send_packet");
						log(lyramilk::log::error,__FUNCTION__) << msg << std::endl;
						throw lyramilk::exception(msg);
					}
					ret = avcodec_receive_frame(pcodec,pframe);
					if(ret < 0){
						if(ret == AVERROR(EAGAIN)) continue;
						lyramilk::data::string msg = D("解码错误%s","avcodec_receive_frame");
						log(lyramilk::log::error,__FUNCTION__) << msg << std::endl;
						throw lyramilk::exception(msg);
					}
					if(pframe->pict_type == AV_PICTURE_TYPE_I){
						sws_scale(sws , (const uint8_t* const*)pframe->data, pframe->linesize,0, pcodec->height, pframeRGB->data, pframeRGB->linesize);
						isok = true;
						timestamp = packet.pts * av_q2d(pmaster->pformat->streams[videoindex]->time_base);
						break;
					}
				}
				av_packet_unref(&packet);
			}
			return isok;
		}


		lyramilk::data::var time(lyramilk::data::var::array args,lyramilk::data::var::map env)
		{
			return timestamp;
		}

		static int define(lyramilk::script::engine* p)
		{
			lyramilk::script::engine::functional_map fn;
			fn["ok"] = lyramilk::script::engine::functional<Mpeg_KeyFrame,&Mpeg_KeyFrame::ok>;
			fn["saveJpeg"] = lyramilk::script::engine::functional<Mpeg_KeyFrame,&Mpeg_KeyFrame::save_jpeg>;
			fn["time"] = lyramilk::script::engine::functional<Mpeg_KeyFrame,&Mpeg_KeyFrame::time>;
			fn["next"] = lyramilk::script::engine::functional<Mpeg_KeyFrame,&Mpeg_KeyFrame::next>;
			p->define("Mpeg.Keyframe",fn,Mpeg_KeyFrame::ctr,Mpeg_KeyFrame::dtr);
			return 1;
		}
	};


	static int define(lyramilk::script::engine* p)
	{
		int i = 0;
		i+= Mpeg::define(p);
		i+= Mpeg_KeyFrame::define(p);
		return i;
	}


	static __attribute__ ((constructor)) void __init()
	{
		av_register_all();
		avformat_network_init();
		lyramilk::teapoy::script2native::instance()->regist("media.mpeg",define);
	}
}}}


#include <string>
#include <iostream>

#include <windows.h>

#include "streamreader.h"

static enum AVPixelFormat get_hw_format(AVCodecContext* avctx, const enum AVPixelFormat* pix_fmts)
{
	while (*pix_fmts != AV_PIX_FMT_NONE) {
		if (*pix_fmts == AV_PIX_FMT_QSV) {
			AVHWFramesContext* frames_ctx;
			AVQSVFramesContext* frames_hwctx;
			int ret;

			/* create a pool of surfaces to be used by the decoder */
			avctx->hw_frames_ctx = av_hwframe_ctx_alloc(avctx->hw_device_ctx);
			if (!avctx->hw_frames_ctx)
				return AV_PIX_FMT_NONE;
			frames_ctx = (AVHWFramesContext*)avctx->hw_frames_ctx->data;
			frames_hwctx = (AVQSVFramesContext*)frames_ctx->hwctx;

			frames_ctx->format = AV_PIX_FMT_QSV;
			frames_ctx->sw_format = avctx->sw_pix_fmt;
			frames_ctx->width = FFALIGN(avctx->coded_width, 32);
			frames_ctx->height = FFALIGN(avctx->coded_height, 32);
			frames_ctx->initial_pool_size = 32;

			frames_hwctx->frame_type = MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET;

			ret = av_hwframe_ctx_init(avctx->hw_frames_ctx);
			if (ret < 0)
				return AV_PIX_FMT_NONE;

			return AV_PIX_FMT_QSV;
		}

		pix_fmts++;
	}

	return AV_PIX_FMT_NONE;
}

static int streamin_interrupt_cb(void* ctx)
{
	int interrupt = 0;
	if (ctx)
	{
		CStreamReader* in = (CStreamReader*) ctx;
		if (in->m_bStop || in->m_bTimeout)
			interrupt = 1;
	}

	return interrupt;
}

int AnnexbToExtradata(uint8_t* annexbBuf, uint8_t* extraBuffer, int length)
{
	int8_t startCode = 3;

	int index = 0;
	int16_t spsSize = 0;
	int spsStart = 0;
	int16_t ppsSize = 0;
	int ppsStart = 0;

	if (annexbBuf[0] == 0x00 && annexbBuf[1] == 0x00 && annexbBuf[2] == 0x01)
	{
		//3字节
		startCode = 3;
	}
	else if (annexbBuf[0] == 0x00 && annexbBuf[1] == 0x00 && annexbBuf[2] == 0x00 && annexbBuf[3] == 0x01)
	{
		//4字节
		startCode = 4;
	}
	else
		return 0;

	index += startCode;
	spsStart = startCode;

	index += startCode;	
	while (index < length - startCode) {
		if (startCode == 3 && (annexbBuf[index] == 0x00 && annexbBuf[index + 1] == 0x00 && annexbBuf[index + 2] == 0x01) ||
			startCode == 4 && (annexbBuf[index] == 0x00 && annexbBuf[index + 1] == 0x00 && annexbBuf[index + 2] == 0x00 && annexbBuf[index + 3] == 0x01))
		{
			spsSize = index - startCode;
			ppsStart = index + startCode;
		}

		index++;
	}

	if (index == length - startCode)
	{
		ppsSize = length - ppsStart;
	}

	//bits
	//	8   version(always 0x01)
	//	8   avc profile(sps[0][1])
	//	8   avc compatibility(sps[0][2])
	//	8   avc level(sps[0][3])
	//	6   reserved(all bits on)      // 即 0xFC | current byte
	//	2   NALULengthSizeMinusOne        // 前缀长度-1 
	//	3   reserved(all bits on)      // 即 0xE0 | currrent byte
	//	5   number of SPS NALUs(usually 1)
	//	--repeated once per SPS--
	//	16  SPS size
	//	N   variable SPS NALU data
	//	8   number of PPS NALUs(usually 1)
	//	--repeated once per PPS--
	//	16  PPS size
	//	N   variable PPS NALU data

	extraBuffer[0] = 0x01;
	extraBuffer[1] = annexbBuf[1 + startCode];
	extraBuffer[2] = annexbBuf[2 + startCode];
	extraBuffer[3] = annexbBuf[3 + startCode];
	extraBuffer[4] = 0xfc | 0x03;
	extraBuffer[5] = 0xe1;
	extraBuffer[6] = spsSize >> 8 & 0xFF;
	extraBuffer[7] = spsSize & 0xFF;
	memcpy(extraBuffer + 8, annexbBuf + spsStart, spsSize);
	extraBuffer[8 + spsSize] = 0x01;
	extraBuffer[9 + spsSize] = ppsSize >> 8 & 0xFF;
	extraBuffer[10 + spsSize] = ppsSize & 0xFF;
	memcpy(extraBuffer + 11 + spsSize, annexbBuf + ppsStart, ppsSize);

	return spsSize + ppsSize +12;
}

int AnnexbToMp4(uint8_t* annexbBuf, uint8_t* avccBuffer, int length) 
{
	int avccStart = 0;
	int annexbStart = 0;
	int index = 0;
	int startCode = 3;
	int nalSize = 0;
	int frames = 0;

	if (annexbBuf[index] == 0x00 && annexbBuf[index + 1] == 0x00 && annexbBuf[index + 2] == 0x01) 
	{
		//3字节
		startCode = 3;
	}
	else if (annexbBuf[index] == 0x00 && annexbBuf[index + 1] == 0x00 && annexbBuf[index + 2] == 0x00 && annexbBuf[index + 3] == 0x01) 
	{
		//4字节
		startCode = 4;
	}
	else
		return 0;

	index += startCode;
	annexbStart += startCode;

	while (index < length - startCode) {
		if ( startCode == 3 && (annexbBuf[index] == 0x00 && annexbBuf[index + 1] == 0x00 && annexbBuf[index + 2] == 0x01) ||
			startCode == 4 && (annexbBuf[index] == 0x00 && annexbBuf[index + 1] == 0x00 && annexbBuf[index + 2] == 0x00 && annexbBuf[index + 3] == 0x01)) 
		{
			nalSize = index - annexbStart;

			avccBuffer[avccStart] = nalSize >> 24 & 0xFF;
			avccBuffer[avccStart + 1] = nalSize >> 16 & 0xFF;
			avccBuffer[avccStart + 2] = nalSize >> 8 & 0xFF;
			avccBuffer[avccStart + 3] = nalSize & 0xFF;

			memcpy(avccBuffer + 4 + avccStart, annexbBuf + annexbStart, nalSize);

			avccStart += nalSize + 4;
			annexbStart += nalSize + startCode;
			frames++;
		}

		index++;
	}

	if (index == length - startCode)
	{
		// no tail start code found, so treat previous data as a complete nal.
		nalSize = index - annexbStart + startCode;

		avccBuffer[avccStart] = nalSize >> 24 & 0xFF;
		avccBuffer[avccStart + 1] = nalSize >> 16 & 0xFF;
		avccBuffer[avccStart + 2] = nalSize >> 8 & 0xFF;
		avccBuffer[avccStart + 3] = nalSize & 0xFF;

		memcpy(avccBuffer + 4 + avccStart, annexbBuf + annexbStart, nalSize);
		frames++;
	}

	return frames;
}

CStreamReader::CStreamReader()
{
	//av_register_all();
	//avformat_network_init();
}

CStreamReader::~CStreamReader()
{
	//avformat_network_deinit();
}

bool CStreamReader::Open(const string url, bool listen)
{
	m_InputFile = url;

	m_VideoStreamID = -1;
	m_AudioStreamID = -1;

	m_Params.hasAudio_ = false;
	m_Params.hasVideo_ = false;

	m_Params.audioSamplerate_ = 0;
	m_Params.audioBitrate_ = 0;
	m_Params.audioChannels_ = 0;
	m_Params.audioFormat_ = AV_SAMPLE_FMT_NONE;
	m_Params.audioCodec_ = AV_CODEC_ID_NONE;

	m_Params.videoFramerate_ = 0;
	m_Params.videoBitrate_ = 0;
	m_Params.videoWidth_ = 0;
	m_Params.videoHeight_ = 0;
	m_Params.videoCodec_ = AV_CODEC_ID_NONE;

	m_Params.streams_ = 0;
	m_Params.duration_ = 0;

	m_pFormatContext = avformat_alloc_context();

	if (m_InputFile.find(m_RTSP) == string::npos)
	{
		if (m_InputFile.find(m_RTMP) == string::npos)
			m_SourceType = Source_Type_File;
		else
			m_SourceType = Source_Type_RTMP;

		if (avformat_open_input(&m_pFormatContext, m_InputFile.c_str(), 0, 0) < 0)
		{
			cout << "Could not open input file." << endl;
			return false;
		}
	}
	else
	{
		//设置参数
		AVDictionary* format_opts = NULL;

		if (listen)
		{
			m_SourceType = Source_Type_RTSP_Server;
			av_dict_set(&format_opts, "rtsp_flags", "listen", 0); // rtsp listen mode
		}
		else
		{
			m_SourceType = Source_Type_RTSP;
			av_dict_set(&format_opts, "rtsp_transport", "tcp", 0); //设置推流的方式，默认udp。
			av_dict_set(&format_opts, "bfs", "extract_extradata", 0);
			//av_dict_set(&format_opts, "genpts", "10", 0);
		}

		av_dict_set(&format_opts, "stimeout", std::to_string(2 * 1000000).c_str(), 0); //设置链接超时时间（us）

		//打开输入流。
		if (avformat_open_input(&m_pFormatContext, m_InputFile.c_str(), NULL, &format_opts) < 0)
		{
			cout << "Could not open input file." << endl;
			return false;
		}

		av_format_inject_global_side_data(m_pFormatContext);

		AVIOInterruptCB int_cb = { streamin_interrupt_cb, this };
		m_pFormatContext->interrupt_callback = int_cb;
	}

	if (avformat_find_stream_info(m_pFormatContext, 0) < 0)
	{
		cout << "Failed to retrieve input stream information." << endl;
		return false;
	}

	m_Params.fileName_ = m_InputFile;

	for (int i = 0; i < m_pFormatContext->nb_streams; i++)
	{
		if (m_pFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			m_VideoStreamID = i;
			m_Params.hasVideo_ = true;

			m_Params.videoWidth_ = m_pFormatContext->streams[m_VideoStreamID]->codecpar->width;
			m_Params.videoHeight_ = m_pFormatContext->streams[m_VideoStreamID]->codecpar->height;

			if (m_pFormatContext->streams[m_VideoStreamID]->avg_frame_rate.den != 0)
				m_Params.videoFramerate_ = m_pFormatContext->streams[m_VideoStreamID]->avg_frame_rate.num / m_pFormatContext->streams[m_VideoStreamID]->avg_frame_rate.den;
			else
				m_Params.videoFramerate_ = 1;

			m_Params.videoBitrate_ = m_pFormatContext->streams[m_VideoStreamID]->codecpar->bit_rate;
			m_Params.pixelFormat_ = (AVPixelFormat) m_pFormatContext->streams[m_VideoStreamID]->codecpar->format;
			m_Params.videoCodec_ = m_pFormatContext->streams[m_VideoStreamID]->codecpar->codec_id;

			//char name[AV_FOURCC_MAX_STRING_SIZE] = "\0";
			//av_fourcc_make_string(name, m_pFormatContext->streams[m_VideoStreamID]->codecpar->codec_tag);
			//cout << "FourCC: " << name << endl;
		}
		else if (m_pFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			m_AudioStreamID = i;
			m_Params.hasAudio_ = true;

			m_Params.audioSamplerate_ = m_pFormatContext->streams[m_AudioStreamID]->codecpar->sample_rate;
			m_Params.audioBitrate_ = m_pFormatContext->streams[m_AudioStreamID]->codecpar->bit_rate;
			m_Params.audioChannels_ = m_pFormatContext->streams[m_AudioStreamID]->codecpar->channels;
			m_Params.audioFormat_ = (AVSampleFormat) m_pFormatContext->streams[m_AudioStreamID]->codecpar->format;
			m_Params.audioCodec_ = m_pFormatContext->streams[m_AudioStreamID]->codecpar->codec_id;
		}
	}

	if ((m_AudioStreamID == -1) && (m_VideoStreamID == -1))
	{
		return false; // no audio or video stream in source
	}

	m_Params.duration_ = m_pFormatContext->duration / 1000000.0;

#if _DEBUG
	cout << "\n==============Input Video=============" << endl;
	av_dump_format(m_pFormatContext, 0, url.c_str(), 0);
#endif

	if (m_OutputWidth == 0 || m_OutputHeight == 0)
	{
		m_OutputWidth = m_Params.videoWidth_;
		m_OutputHeight = m_Params.videoHeight_;
		m_OutputFormat = m_Params.pixelFormat_;
	}
	else
	{
		if (m_OutputWidth != m_Params.videoWidth_ || m_OutputHeight != m_Params.videoHeight_ || m_OutputFormat != m_Params.pixelFormat_)
			m_bConvertOutput = true;
	}

	if (m_pCallback)
	{
		m_pCallback->OnOpen(m_Params, m_OutputWidth, m_OutputHeight, m_OutputFormat);

		if (m_Params.hasAudio_)
			m_pCallback->OnAudioExtraData(m_pFormatContext->streams[m_AudioStreamID]->codecpar->extradata, m_pFormatContext->streams[m_AudioStreamID]->codecpar->extradata_size);
		
		if (m_Params.hasVideo_)
			m_pCallback->OnVideoExtraData(m_pFormatContext->streams[m_VideoStreamID]->codecpar->extradata, m_pFormatContext->streams[m_VideoStreamID]->codecpar->extradata_size);

	}

	if (m_SourceType == Source_Type_RTSP_Server)
		Start(true, true, false);

	return true;
}

bool CStreamReader::Start(bool loop, bool decodeVideo, bool decodeAudio)
{
#if _DEBUG
	cout << "Start." << endl;
#endif

	m_bLoop = loop;
	m_bDecodeVideo = decodeVideo;
	m_bDecodeAudio = decodeAudio;
	m_bStop = false;
	m_bTimeout = false;
	m_LastActiveTime = 0;

	if (m_Params.hasVideo_ && m_bDecodeVideo)
	{
		AVCodec* dec = nullptr;
		int rc = 0;
		bool bH264 = (m_pFormatContext->streams[m_VideoStreamID]->codecpar->codec_id == AV_CODEC_ID_H264);
		bool bH265 = (m_pFormatContext->streams[m_VideoStreamID]->codecpar->codec_id == AV_CODEC_ID_HEVC);

		if (bH264 || bH265)
		{
			if (m_bEnableHWDecode)
			{
				if (bH264)
					dec = avcodec_find_decoder_by_name("h264_qsv");
				else if (bH265)
					dec = avcodec_find_decoder_by_name("hevc_qsv");

				// hw decoder failed, try sw decoder;
				if (!dec)
				{
					dec = avcodec_find_decoder(m_pFormatContext->streams[m_VideoStreamID]->codecpar->codec_id);
				}
				else
				{
					m_bQSV = true;
				}
			}
			else
			{
				dec = avcodec_find_decoder(m_pFormatContext->streams[m_VideoStreamID]->codecpar->codec_id);
			}
		}
		else
		{
			dec = avcodec_find_decoder(m_pFormatContext->streams[m_VideoStreamID]->codecpar->codec_id);
		}


		// find decoder for the stream
		if (!dec)
		{
			cout << "Cannot find video decoder." << endl;
			return false;
		}

		// Allocate a codec context for the decoder
		m_VideoDecodeContext = avcodec_alloc_context3(dec);
		if (!m_VideoDecodeContext)
		{
			cout << "Cannot open video decoder." << endl;
			return false;
		}

		av_opt_set(m_VideoDecodeContext->priv_data, "tune", "zerolatency", 0);

		// Copy codec parameters from input stream to output codec context
		if ((rc = avcodec_parameters_to_context(m_VideoDecodeContext, m_pFormatContext->streams[m_VideoStreamID]->codecpar)) < 0)
		{
			cout << "Cannot get video decoder parameters." << endl;
			return false;
		}

		if (m_bQSV && m_bEnableHWDecode)
		{
//			m_VideoDecodeContext->pix_fmt = AV_PIX_FMT_QSV;
			m_VideoDecodeContext->get_format = get_hw_format;

			if ((rc = av_hwdevice_ctx_create(&m_HWDeviceContext, AV_HWDEVICE_TYPE_QSV, "auto", NULL, 0)) < 0) {
				//OutputDebugString("HW Decoder failed.\n");
				cout << "Cannot open HW decoder codec." << endl;
				return false;
			}
			m_VideoDecodeContext->hw_device_ctx = av_buffer_ref(m_HWDeviceContext);
		}

		if (m_SourceType == Source_Type_RTSP)
		{
			uint8_t* extradata = (uint8_t*) av_mallocz(m_VideoDecodeContext->extradata_size + 12 + AV_INPUT_BUFFER_PADDING_SIZE);
			int extradata_size = AnnexbToExtradata(m_VideoDecodeContext->extradata, extradata, m_VideoDecodeContext->extradata_size);
			
			if (extradata_size > 0)
			{
				av_free(m_VideoDecodeContext->extradata);
				m_VideoDecodeContext->extradata = extradata;
				m_VideoDecodeContext->extradata_size = extradata_size;
			}
		}

		// Init the decoders, with or without reference counting
		if ((rc = avcodec_open2(m_VideoDecodeContext, dec, NULL)) < 0)
		{
			cout << "Cannot open video decoder codec." << endl;
			return false;
		}
	}

	if (m_Params.hasAudio_ && m_bDecodeAudio)
	{
		AVCodec* dec = nullptr;
		int rc = 0;

		// find decoder for the stream
		dec = avcodec_find_decoder(m_pFormatContext->streams[m_AudioStreamID]->codecpar->codec_id);
		if (!dec)
		{
			cout << "Cannot find audio decoder." << endl;
			return false;
		}

		// Allocate a codec context for the decoder
		m_AudioDecodeContext = avcodec_alloc_context3(dec);
		if (!m_AudioDecodeContext)
		{
			cout << "Cannot open audio decoder." << endl;
			return false;
		}

		// Copy codec parameters from input stream to output codec context
		if ((rc = avcodec_parameters_to_context(m_AudioDecodeContext, m_pFormatContext->streams[m_AudioStreamID]->codecpar)) < 0)
		{
			cout << "Cannot get audio decoder parameters." << endl;
			return false;
		}

		m_AudioDecodeContext->request_channel_layout = AV_CH_LAYOUT_STEREO;
		m_AudioDecodeContext->request_sample_fmt = AV_SAMPLE_FMT_S16;

		// Init the decoders, with or without reference counting
		if ((rc = avcodec_open2(m_AudioDecodeContext, dec, NULL)) < 0)
		{
			cout << "Cannot open audio decoder codec." << endl;
			return false;
		}
	}

	if (m_pThread)
	{
		delete m_pThread;
		m_pThread = nullptr;
	}

	m_pThread = new boost::thread(boost::bind(&CStreamReader::DemuxingThread, this));

	if (!m_pThread)
		return false;

	if (m_pTimeoutThread)
	{
		delete m_pTimeoutThread;
		m_pTimeoutThread = nullptr;
	}

	if (m_SourceType == Source_Type_RTSP_Server)
	{
		m_pTimeoutThread = new boost::thread(boost::bind(&CStreamReader::TimeoutCheckThread, this));

		if (!m_pTimeoutThread)
			return false;
	}

	if (m_pCallback)
		m_pCallback->OnStarted();

	return true;
}

bool CStreamReader::Stop()
{
#if _DEBUG
	cout << "Stop." << endl;
#endif

	m_bStop = true;

	cout << "wait time out thread exit." << endl;

	if (m_pTimeoutThread)
	{
		m_pTimeoutThread->interrupt();
		m_pTimeoutThread->join();
		delete m_pTimeoutThread;
		m_pTimeoutThread = nullptr;
	}

	cout << "time out thread end." << endl;

	if (m_pThread)
	{
		//		m_pThread->interrupt();
		m_pThread->join();
		delete m_pThread;
		m_pThread = nullptr;
	}

	m_bTimeout = false;

	return true;
}

bool CStreamReader::Close()
{
#if _DEBUG
	cout << "Close." << endl;
#endif

	avcodec_free_context(&m_VideoDecodeContext);
	avcodec_free_context(&m_AudioDecodeContext);
	avformat_close_input(&m_pFormatContext);
	avformat_free_context(m_pFormatContext);

	av_buffer_unref(&m_HWDeviceContext);

	return true;
}

void CStreamReader::DemuxingThread()
{
	//	av_register_all();

	int aframe_count = 0;
	int vframe_count = 0;

	uint64_t time_stamp = 0;
	m_StartTime = av_gettime();
	m_PlayTime = 0;

	AVRational time_base_q = { 1, AV_TIME_BASE };
	AVFrame* frame = av_frame_alloc();
	AVFrame* frameSW = av_frame_alloc();
	AVFrame* frame420P = av_frame_alloc();
	AVFrame* frameOut = av_frame_alloc();

	uint8_t* video_dst_data[4] = { nullptr, nullptr, nullptr, nullptr };
	int      video_dst_linesize[4] = { 0, 0, 0, 0 };
	int		 video_dst_bufsize = 0;

	uint8_t** audio_dst_data;
	int      audio_dst_linesize = 0;
	int64_t dst_nb_samples = 0;

	int64_t ts = 0;

	int ret = 0;
	bool bReady = true;
	SwsContext* sws_ctx = nullptr;
	SwsContext* sws_ctx_out = nullptr;
	SwrContext* swr_ctx = nullptr;

	AVBSFContext* bsf_ctx = nullptr;

	bool bResize = false;

	if (m_bDecodeVideo)
	{
		ret = av_image_alloc(video_dst_data, video_dst_linesize, m_OutputWidth, m_OutputHeight, m_OutputFormat, 1);
		if (ret < 0)
		{
			cout << "Could not allocate raw video buffer1." << endl;
			bReady = false;
		}
		else
		{
			video_dst_bufsize = ret;

			if (m_bConvertOutput)
			{
				float srcRatio = (float) m_Params.videoWidth_ / (float)m_Params.videoHeight_;
				float dstRatio = (float) m_OutputWidth / (float) m_OutputHeight;

				if (srcRatio == dstRatio || m_OutputType == Video_Resize)
				{
					bResize = true;
				}

				if (bResize)
				{
					sws_ctx_out = sws_getContext(m_Params.videoWidth_, m_Params.videoHeight_, m_Params.pixelFormat_,
						m_OutputWidth, m_OutputHeight, m_OutputFormat, SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);

					ret = av_image_alloc(frameOut->data, frameOut->linesize, m_OutputWidth, m_OutputHeight, m_OutputFormat, 1);
					if (ret < 0)
					{
						cout << "Could not allocate raw video buffer2." << endl;
						bReady = false;
					}
				}
				else
				{
					char args[512];
					memset(args, 0, 512);

					if (srcRatio > dstRatio)
					{
						// source is wider
						if (m_OutputType == Video_Crop)
						{
							snprintf(args, sizeof(args), "scale=%d:%d,crop=%d:%d:(iw-ow)/2:0", -1, m_OutputHeight, m_OutputWidth, m_OutputHeight);
						}
						else
						{
							snprintf(args, sizeof(args), "scale=%d:%d,pad=%d:%d:0:oh/2", m_OutputWidth, -1, m_OutputWidth, m_OutputHeight);
						}
					}
					else
					{
						// source is heigher
						if (m_OutputType == Video_Crop)
						{
							snprintf(args, sizeof(args), "scale=%d:%d,crop=%d:%d:0:(ih-oh)/2", m_OutputWidth, -1, m_OutputWidth, m_OutputHeight);
						}
						else
						{
							snprintf(args, sizeof(args), "scale=%d:%d,pad=%d:%d:ow/2:0", -1, m_OutputHeight, m_OutputWidth, m_OutputHeight);
						}
					}

					ret = InitFilters(args);
					if (ret < 0)
					{
						cout << "Falied to init filter graph." << endl;
						bReady = false;
					}
				}
			}

			if (bReady && m_bQSV && m_bEnableHWDecode)
			{
				cout << "Enable QSV Hardware Decoder." << endl;
				//OutputDebugString("Enable QSV Hardware Decoder.\n");

				sws_ctx = sws_getContext(m_Params.videoWidth_, m_Params.videoHeight_, AV_PIX_FMT_NV12,
					m_Params.videoWidth_, m_Params.videoHeight_, AV_PIX_FMT_YUV420P, SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);

				ret = av_image_alloc(frame420P->data, frame420P->linesize, m_Params.videoWidth_, m_Params.videoHeight_, AV_PIX_FMT_YUV420P, 4);
				if (ret < 0)
				{
					cout << "Could not allocate raw video buffer3." << endl;
					bReady = false;
				}
			}
		}
	}
	else
	{
		const AVBitStreamFilter* filter = av_bsf_get_by_name("h264_mp4toannexb");

		if (!filter) {
			cout << "Unknow bitstream filter." << endl;
			bReady = false;
		}
		else
		{
			if ((ret = av_bsf_alloc(filter, &bsf_ctx) < 0)) {
				cout << "av_bsf_alloc failed" << endl;
				bReady = false;
			}
			else
			{
				if ((ret = avcodec_parameters_copy(bsf_ctx->par_in, m_pFormatContext->streams[m_VideoStreamID]->codecpar)) < 0) {
					cout << "avcodec_parameters_copy failed." << endl;
					bReady = false;
				}
				else
				{
					if ((ret = av_bsf_init(bsf_ctx)) < 0) {
						cout << "av_bsf_init failed." << endl;
						bReady = false;
					}
				}
			}
		}
	}

	if (m_bDecodeAudio)
	{
		if (m_Params.audioFormat_ != AV_SAMPLE_FMT_S16 || m_Params.audioSamplerate_ != 44100 || m_AudioDecodeContext->channel_layout != AV_CH_LAYOUT_STEREO)
		{
			swr_ctx = swr_alloc();
			if (!swr_ctx)
			{
				cout << "swr_alloc failed." << endl;
				bReady = false;
			}
			else
			{
				/* set options */
				av_opt_set_int(swr_ctx, "in_channel_layout", m_AudioDecodeContext->channel_layout, 0);
				av_opt_set_int(swr_ctx, "in_sample_rate", m_Params.audioSamplerate_, 0);
				av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", m_Params.audioFormat_, 0);

				av_opt_set_int(swr_ctx, "out_channel_layout", AV_CH_LAYOUT_STEREO, 0);
				av_opt_set_int(swr_ctx, "out_sample_rate", 44100, 0);
				av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);

				/* initialize the resampling context */
				if ((ret = swr_init(swr_ctx)) < 0) {
					cout << "swr_init failed." << endl;
					bReady = false;
				}
				else
				{
					dst_nb_samples = av_rescale_rnd(m_AudioDecodeContext->frame_size, 44100, m_Params.audioSamplerate_, AV_ROUND_UP);
					if ( (ret = av_samples_alloc_array_and_samples(&audio_dst_data, &audio_dst_linesize, 2, dst_nb_samples, AV_SAMPLE_FMT_S16, 0)) < 0)
					{
						cout << "allocate audio buffer failed." << endl;
						bReady = false;
					}
				}
			}

		}
	}

	while (!m_bStop && bReady)
	{
		//Get an AVPacket
		if (av_read_frame(m_pFormatContext, &m_Packet) < 0)
		{
			if (m_bLoop && (m_SourceType == Source_Type_File))
			{
				m_StartTime = av_gettime();
				m_PlayTime = time_stamp;

				cout << "Media file loop back." << endl;

				// play to end, replay in loop mode
				av_seek_frame(m_pFormatContext, m_AudioStreamID, 0, AVSEEK_FLAG_ANY);
				continue;
			}
			else
				break;
		}

		if (m_Packet.stream_index == m_VideoStreamID)
		{
			//设置 PTS
			if (m_Packet.pts == AV_NOPTS_VALUE)
			{
				//原始两帧之间的时间
				int64_t calc_duration = (double)AV_TIME_BASE / av_q2d(m_pFormatContext->streams[m_VideoStreamID]->r_frame_rate);

				//设置一帧的时间参数
				AVRational time_base1 = m_pFormatContext->streams[m_VideoStreamID]->time_base;
				m_Packet.pts = (double)(vframe_count * calc_duration) / (double)(av_q2d(time_base1) * AV_TIME_BASE);
				m_Packet.dts = m_Packet.pts;
				m_Packet.duration = (double)calc_duration / (double)(av_q2d(time_base1) * AV_TIME_BASE);
			}

			if (m_SourceType == Source_Type_File)
			{
				ts = m_Packet.dts;
			}
			else
			{
				ts = m_Packet.pts;
			}

			if (ts < 0)
				ts = 0;

			time_stamp = av_rescale_q(ts, m_pFormatContext->streams[m_VideoStreamID]->time_base, time_base_q);

			if (m_SourceType == Source_Type_File)
			{
				//帧延时
				int64_t now_time = av_gettime() - m_StartTime;

				if (time_stamp > now_time)
					av_usleep(time_stamp - now_time);
			}
			else
			{
				if (m_bDecodeVideo)
				{
					AVPacket* pkt = av_packet_alloc();
					av_new_packet(pkt, m_Packet.size);
					av_packet_copy_props(pkt, &m_Packet);
					int frames = AnnexbToMp4(m_Packet.data, pkt->data, m_Packet.size);
					if (frames > 0)
					{
						av_packet_ref(&m_Packet, pkt);
					}
					else
					{
						av_free(pkt->data);
						av_packet_unref(pkt);
					}
				}
			}

			if (m_pCallback)
			{
				if (m_bDecodeVideo)
				{
					// decode video
					int ret = avcodec_send_packet(m_VideoDecodeContext, &m_Packet);
					int h = 0;
					while (ret >= 0)
					{
						ret = avcodec_receive_frame(m_VideoDecodeContext, frame);
						if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
						{
							// cout << "wait for more data." << endl;
							break;
						}
						else if (ret >= 0) 
						{
							AVFrame* fr = frame;

							if (m_bQSV && m_bEnableHWDecode)
							{
								if (frame->format == AV_PIX_FMT_QSV)
								{
									ret = av_hwframe_transfer_data(frameSW, frame, 0);
									if (ret < 0)
									{
										cout << "Transfer image from GPU to CPU failed." << endl;
										break;
									}

									h = sws_scale(sws_ctx, (uint8_t const* const*)frameSW->data, frameSW->linesize, 0,
										frameSW->height, frame420P->data, frame420P->linesize);

									av_frame_copy_props(frame420P, frame);

									frame420P->width = frameSW->width;
									frame420P->height = frameSW->height;
									frame420P->format = AV_PIX_FMT_YUV420P;
									fr = frame420P;
								}
							}

							if (m_bConvertOutput)
							{
								if (bResize)
								{
									h = sws_scale(sws_ctx_out, fr->data, fr->linesize, 0,
										fr->height, frameOut->data, frameOut->linesize);

									fr = frameOut;
								}
								else
								{
									if (av_buffersrc_add_frame_flags(m_BufferSourceContext, fr, AV_BUFFERSRC_FLAG_KEEP_REF) < 0) 
									{
										cout << "Error while feeding the filtergraph." << endl;
										break;
									}

									/* pull filtered frames from the filtergraph */
									while (1) {
										ret = av_buffersink_get_frame(m_BufferSinkContext, frameOut);
										if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
											break;
										if (ret < 0)
											break;
									}
									fr = frameOut;
								}
							}

							/* copy decoded frame to destination buffer:
							* this is required since rawvideo expects non aligned data */
							av_image_copy(video_dst_data, video_dst_linesize, (const uint8_t**)(fr->data), fr->linesize, m_OutputFormat, m_OutputWidth, m_OutputHeight);
							if (m_bConvertOutput && !bResize)
								av_frame_unref(fr);

							m_LastActiveTime = time_stamp + m_PlayTime;
							m_pCallback->OnFrameData(video_dst_data, (uint32_t *) video_dst_linesize, m_OutputWidth, m_OutputHeight, m_LastActiveTime);
						}
						else
						{
							cout << "Error decoding video frame." << endl;

							break;
						}
					}
				}
				else
				{
					// without decode video
					bool bKeyFrame = m_Packet.flags & AV_PKT_FLAG_KEY;

					if (m_SourceType == Source_Type_RTSP || m_SourceType == Source_Type_RTSP_Server)
					{
						m_LastActiveTime = time_stamp + m_PlayTime;
						m_pCallback->OnVideoData(m_Packet.data, m_Packet.size, m_LastActiveTime, bKeyFrame);
					}
					else
					{
						// mp4 to annexb 
						// change video stream format, replace nal length (int 4 bytes) with start code 0x000001, 3 bytes.

						int ret = 0;
						if (ret = av_bsf_send_packet(bsf_ctx, &m_Packet) >= 0) {
							while ((ret = av_bsf_receive_packet(bsf_ctx, &m_Packet) == 0)) {
								m_LastActiveTime = time_stamp + m_PlayTime;
								m_pCallback->OnVideoData(m_Packet.data, m_Packet.size, m_LastActiveTime, bKeyFrame);
							}
						}
					}
				}
			}

			vframe_count++;
		}
		else if (m_Packet.stream_index == m_AudioStreamID)
		{
			//设置 PTS
			if (m_Packet.pts == AV_NOPTS_VALUE)
			{
				//原始两帧之间的时间
				int64_t calc_duration = (double)AV_TIME_BASE / av_q2d(m_pFormatContext->streams[m_VideoStreamID]->r_frame_rate);

				//设置一帧的时间参数
				AVRational time_base1 = m_pFormatContext->streams[m_AudioStreamID]->time_base;
				m_Packet.pts = (double)(aframe_count * calc_duration) / (double)(av_q2d(time_base1) * AV_TIME_BASE);
				m_Packet.dts = m_Packet.pts;
				m_Packet.duration = (double)calc_duration / (double)(av_q2d(time_base1) * AV_TIME_BASE);
			}

			if (m_SourceType == Source_Type_File)
			{
				ts = m_Packet.dts;
			}
			else
			{
				ts = m_Packet.pts;
			}

			if (ts < 0)
				ts = 0;

			time_stamp = av_rescale_q(ts, m_pFormatContext->streams[m_AudioStreamID]->time_base, time_base_q);

			if (m_SourceType == Source_Type_File)
			{
				//帧延时
				int64_t now_time = av_gettime() - m_StartTime;

				if (time_stamp > now_time)
					av_usleep(time_stamp - now_time);
			}

			if (m_pCallback)
			{
				if (m_bDecodeAudio)
				{
					/* send the packet with the compressed data to the decoder */
					int ret = avcodec_send_packet(m_AudioDecodeContext, &m_Packet);

					/* read all the output frames (in general there may be any number of them */
					while (ret >= 0) {
						ret = avcodec_receive_frame(m_AudioDecodeContext, frame);
						if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
							break;
						else if (ret >= 0) {
							if (swr_ctx)
							{
								ret = swr_convert(swr_ctx, audio_dst_data, dst_nb_samples, (const uint8_t**) frame->data, frame->nb_samples);
								if (ret < 0) {
									break;
								}
							}

							m_LastActiveTime = time_stamp + m_PlayTime;

							m_pCallback->OnPCMData(audio_dst_data[0], audio_dst_linesize, m_LastActiveTime);
						}
						else
						{
							cout << "Error decoding audio frame." << endl;
							break;
						}
					}
				}
				else
				{
					m_LastActiveTime = time_stamp + m_PlayTime;

					m_pCallback->OnAudioData(m_Packet.data, m_Packet.size, m_LastActiveTime);
				}
			}

			aframe_count++;
		}
		else
		{
			continue;
		}

		av_packet_unref(&m_Packet);
	}

	if (m_bDecodeVideo)
	{
		if (video_dst_data[0])
			av_free(video_dst_data[0]);

		if (frame420P->data[0])
			av_free(frame420P->data[0]);
	}

	if (m_bDecodeAudio)
		if (audio_dst_data[0])
			av_freep(&audio_dst_data[0]);

	if (swr_ctx)
		swr_free(&swr_ctx);

	if (bResize && sws_ctx_out)
		sws_freeContext(sws_ctx_out);

	if (sws_ctx)
		sws_freeContext(sws_ctx);

	if (frame)
		av_frame_free(&frame);
	
	if (frameSW)
		av_frame_free(&frameSW);

	if (frame420P)
		av_frame_free(&frame420P);

	if (frameOut)
		av_frame_free(&frameOut);

	if (bsf_ctx)
		av_bsf_free(&bsf_ctx);

	if (m_FilterGraph)
		avfilter_graph_free(&m_FilterGraph);

	if (m_pCallback)
		m_pCallback->OnStopped();

#if _DEBUG
	cout << "End of demuxing thread." << endl;
#endif
}

void CStreamReader::TimeoutCheckThread()
{
	uint64_t last_check_time = m_LastActiveTime;

	while (!m_bStop)
	{
		boost::this_thread::sleep_for(boost::chrono::milliseconds(2000));
		if (last_check_time == m_LastActiveTime)
		{
			m_bTimeout = true;
			cout << "2s recv timeout." << endl;
			break;
		}
		else
			last_check_time = m_LastActiveTime;
	}
#if _DEBUG
	cout << "End of Timeout Check thread." << endl;
#endif
}

int CStreamReader::InitFilters(const char* filters_descr)
{
	char args[512];
	int ret = 0;
	const AVFilter* buffersrc = avfilter_get_by_name("buffer");
	const AVFilter* buffersink = avfilter_get_by_name("buffersink");
	AVFilterInOut* outputs = avfilter_inout_alloc();
	AVFilterInOut* inputs = avfilter_inout_alloc();
	AVRational time_base = m_pFormatContext->streams[m_VideoStreamID]->time_base;
	enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_GRAY8, AV_PIX_FMT_NONE };

	m_FilterGraph = avfilter_graph_alloc();
	if (!outputs || !inputs || !m_FilterGraph) {
		ret = AVERROR(ENOMEM);
		goto end;
	}

	/* buffer video source: the decoded frames from the decoder will be inserted here. */
	snprintf(args, sizeof(args),
		"video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
		m_VideoDecodeContext->width, m_VideoDecodeContext->height, m_VideoDecodeContext->pix_fmt,
		time_base.num, time_base.den,
		m_VideoDecodeContext->sample_aspect_ratio.num, m_VideoDecodeContext->sample_aspect_ratio.den);

	ret = avfilter_graph_create_filter(&m_BufferSourceContext, buffersrc, "in",
		args, NULL, m_FilterGraph);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
		goto end;
	}

	/* buffer video sink: to terminate the filter chain. */
	ret = avfilter_graph_create_filter(&m_BufferSinkContext, buffersink, "out",
		NULL, NULL, m_FilterGraph);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
		goto end;
	}

	//ret = av_opt_set_int_list(m_BufferSinkContext, "pix_fmts", pix_fmts,
	//	AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
	//if (ret < 0) {
	//	av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format\n");
	//	goto end;
	//}

	/*
	 * Set the endpoints for the filter graph. The filter_graph will
	 * be linked to the graph described by filters_descr.
	 */

	 /*
	  * The buffer source output must be connected to the input pad of
	  * the first filter described by filters_descr; since the first
	  * filter input label is not specified, it is set to "in" by
	  * default.
	  */
	outputs->name = av_strdup("in");
	outputs->filter_ctx = m_BufferSourceContext;
	outputs->pad_idx = 0;
	outputs->next = NULL;

	/*
	 * The buffer sink input must be connected to the output pad of
	 * the last filter described by filters_descr; since the last
	 * filter output label is not specified, it is set to "out" by
	 * default.
	 */
	inputs->name = av_strdup("out");
	inputs->filter_ctx = m_BufferSinkContext;
	inputs->pad_idx = 0;
	inputs->next = NULL;

	if ((ret = avfilter_graph_parse_ptr(m_FilterGraph, filters_descr,
		&inputs, &outputs, NULL)) < 0)
		goto end;

	if ((ret = avfilter_graph_config(m_FilterGraph, NULL)) < 0)
		goto end;

end:
	avfilter_inout_free(&inputs);
	avfilter_inout_free(&outputs);

	return ret;
}

//void CStreamReader::ScreenCaptureThread()
//{
//	uint8_t* frameRGB32 = new uint8_t[];
//	uint8_t* frameI420 = new uint8_t[];;
//
//	SwsContext* sws_ctx = nullptr;
//
//#if _DEBUG
//	cout << "End of Screen Capture thread." << endl;
//#endif
//
//}
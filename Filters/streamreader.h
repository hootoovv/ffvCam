#pragma once

#ifdef WIN32
#include "inttypes.h"
#endif
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/pixdesc.h"
#include "libavutil/fifo.h"
#include "libavutil/intreadwrite.h"
#include "libavutil/opt.h"
#include "libavutil/time.h"
#include "libswresample/swresample.h"
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
#include "libavutil/imgutils.h"
#include "libavutil/hwcontext.h"
#include "libavutil/hwcontext_qsv.h"
}

#include "boost/thread.hpp"

using namespace std;

struct StreamParams
{
	bool hasAudio_ = false;
	bool hasVideo_ = false;

	int audioSamplerate_ = 0;
	int audioBitrate_ = 0;
	int audioChannels_ = 0;
	AVSampleFormat audioFormat_ = AV_SAMPLE_FMT_NONE;
	AVCodecID audioCodec_ = AV_CODEC_ID_NONE;

	int videoFramerate_ = 0;
	int videoBitrate_ = 0;
	int	videoWidth_ = 0;
	int	videoHeight_ = 0;
	AVPixelFormat pixelFormat_ = AV_PIX_FMT_NONE;
	AVCodecID videoCodec_ = AV_CODEC_ID_NONE;

	int streams_ = 0;
	double duration_ = 0;
	string fileName_ = "";
};

class IStreamReaderCallback
{
public:
	virtual void OnOpen(StreamParams sp, uint32_t w, uint32_t h, int pix_fmt) = 0;
	virtual void OnStarted() = 0;
	virtual void OnStopped() = 0;

	// Compressed data stream (encoded)
	virtual void OnAudioExtraData(uint8_t* data, int len) = 0;
	virtual void OnVideoExtraData(uint8_t* data, int len) = 0;
	virtual void OnAudioData(uint8_t* data, int len, int64_t ts) = 0;
	virtual void OnVideoData(uint8_t* data, int len, int64_t ts, bool key) = 0;

	// Raw data stream (decoded)
	virtual void OnFrameData(uint8_t** data, uint32_t* linesize, uint32_t w, uint32_t h, int64_t ts) = 0;
	virtual void OnPCMData(uint8_t* data, uint32_t len, int64_t ts) = 0;
};

enum Source_Type
{
	Source_Type_File,
	Source_Type_RTSP,
	Source_Type_RTMP,
	Source_Type_RTSP_Server,
	Source_Type_Desktop
};

enum Convert_Type
{
	Video_Pad,
	Video_Crop,
	Video_Resize
};

class CStreamReader
{
public:
	CStreamReader();
	virtual ~CStreamReader();

public:
	bool Open(const string url, bool listen = false);
	bool Close();
	bool Start(bool loop = true, bool decodeVideo = false, bool decodeAudio = false);
	bool Stop();
	void EnableHWDecode(bool enable = true) { m_bEnableHWDecode = enable; };

	void SetCallback(IStreamReaderCallback* cb) { m_pCallback = cb; };
	void SetVideoOutput(uint32_t w, uint32_t h, Convert_Type mode = Video_Pad, AVPixelFormat format = AV_PIX_FMT_YUV420P) {
		m_OutputWidth = w;
		m_OutputHeight = h;
		m_OutputFormat = format;
		m_OutputType = mode;
	};

	bool m_bStop = false;
	bool m_bTimeout = false;
	int64_t m_LastActiveTime = 0;
	Source_Type m_SourceType = Source_Type_File;

private:
	AVFormatContext* m_pFormatContext = nullptr;
	AVCodecContext* m_VideoDecodeContext = nullptr;
	AVCodecContext* m_AudioDecodeContext = nullptr;
	AVBufferRef* m_HWDeviceContext = nullptr;

	AVFilterContext* m_BufferSinkContext = nullptr;
	AVFilterContext* m_BufferSourceContext = nullptr;
	AVFilterGraph* m_FilterGraph = nullptr;


	AVPacket m_Packet;

	string m_InputFile = "";
	int m_VideoStreamID = -1;
	int m_AudioStreamID = -1;
	const string m_RTMP = "rtmp://";
	const string m_RTSP = "rtsp://";

	boost::thread* m_pThread = nullptr;
	boost::thread* m_pTimeoutThread = nullptr;
	//boost::thread* m_pCaptureThread = nullptr;

	bool m_bLoop = true;
	bool m_bDecodeVideo = false;
	bool m_bDecodeAudio = false;
	bool m_bQSV = false;
	bool m_bEnableHWDecode = false;

	int64_t m_StartTime = 0;
	int64_t m_PlayTime = 0;

	StreamParams m_Params;

	uint32_t		m_OutputWidth = 0;
	uint32_t		m_OutputHeight = 0;
	AVPixelFormat	m_OutputFormat = AV_PIX_FMT_YUV420P;
	Convert_Type	m_OutputType = Video_Pad;
	bool			m_bConvertOutput = false;

protected:
	IStreamReaderCallback* m_pCallback = nullptr;

private:
	int InitFilters(const char* filters_descr);
	void DemuxingThread();
	void TimeoutCheckThread();
	//void ScreenCaptureThread();

};


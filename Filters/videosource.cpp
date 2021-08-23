#include <boost/algorithm/string.hpp>

#include "videosource.h"

using namespace std;
using namespace boost;

void CVideoSource::OnOpen(StreamParams sp, uint32_t w, uint32_t h, int pix_fmt)
{
	if (sp.hasVideo_)
	{
		width_ = w;
		height_ = h;
		format_ = pix_fmt;
	}
}

void CVideoSource::OnStarted()
{
	running_ = true;
}

void CVideoSource::OnStopped()
{
	running_ = false;
}

void CVideoSource::OnAudioExtraData(uint8_t* data, int len)
{
}

void CVideoSource::OnVideoExtraData(uint8_t* data, int len)
{
}

void CVideoSource::OnAudioData(uint8_t* data, int len, int64_t ts)
{
}

void CVideoSource::OnVideoData(uint8_t* data, int len, int64_t ts, bool key)
{
}

void CVideoSource::OnFrameData(uint8_t** data, uint32_t* linesize, uint32_t w, uint32_t h, int64_t ts)
{
	if (frame_)
	{
		int pos = 0;
		memcpy(frame_, data[0], linesize[0]*h);
		pos = linesize[0] * h;
		memcpy(frame_ + pos, data[1], linesize[1]*h/2);
		pos += linesize[1] * h/2;
		memcpy(frame_ + pos, data[2], linesize[2]*h/2);
		//pos += linesize[2] * h/2;
	}
}

void CVideoSource::OnPCMData(uint8_t* data, uint32_t len, int64_t ts)
{
}

bool CVideoSource::Check(string source, int* w, int* h, int* format)
{
	CStreamReader tmp;
	tmp.SetCallback(this);

	bool rc = tmp.Open(source);

	if (rc)
	{
		*w = width_;
		*h = height_;
		*format = format_;

		// clean up
		tmp.Close();
		width_ = 1280;
		height_ = 720;
		format_ = 0;
	}
	else
		return rc;

	return true;
}

bool CVideoSource::Start(string source, bool loop, bool reconnect, bool hwDecode, bool resize, int w, int h, int mode)
{
	source_ = source;
	loop_ = loop;
	reconnect_ = reconnect;
	hwDecode_ = hwDecode;

	reader_.SetCallback(this);
	reader_.EnableHWDecode(hwDecode_);

	if ((starts_with(source_, "rtsp://") || starts_with(source, "rtmp://")) && reconnect_)
	{
		// this is mainly for rtsp or rtmp stream interruption case.
		// if local file somehow got to stop state, won't restart it anyway. No matter is looped.
		shouldRestart_ = true;
	}

	if (resize)
		reader_.SetVideoOutput(w, h, (Convert_Type)mode);

	bool rc = reader_.Open(source);

	if (rc)
	{
		rc = reader_.Start(loop_, true, false);
	}

	return rc;
}

void CVideoSource::Stop()
{
	if (running_)
	{
		reader_.Stop();
		reader_.Close();
	}
}

bool CVideoSource::shouldRestart()
{
	if (!running_)
		return shouldRestart_;
	else
		return false;
};

bool CVideoSource::shouldStop()
{
	return reader_.m_bTimeout;
}

bool CVideoSource::isRunning()
{
	return running_;
}
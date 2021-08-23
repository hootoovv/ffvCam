#pragma once
#include "streamreader.h"

class CVideoSource : public IStreamReaderCallback
{
public:
    void OnOpen(StreamParams sp, uint32_t w, uint32_t h, int pix_fmt);
    void OnStarted();
    void OnStopped();
    void OnAudioExtraData(uint8_t* data, int len);
    void OnVideoExtraData(uint8_t* data, int len);
    void OnAudioData(uint8_t* data, int len, int64_t ts);
    void OnVideoData(uint8_t* data, int len, int64_t ts, bool key);
    void OnFrameData(uint8_t** data, uint32_t* linesize, uint32_t w, uint32_t h, int64_t ts);
    void OnPCMData(uint8_t* data, uint32_t len, int64_t ts);

    bool Check(string source, int* w, int* h, int* format);

    bool Start(string source, bool loop = true, bool reconnect = true, bool hwDecode = true, bool resize = false, int w = 0, int h = 0, int mode = Video_Pad);
    void Stop();

    bool shouldRestart();
    bool shouldStop();
    bool isRunning();

    std::chrono::system_clock::time_point lastRetry_;
    int retry_ = 0;

private:
    CStreamReader reader_;

    string source_ = "";
    bool loop_ = true;
    bool reconnect_ = true;
    bool hwDecode_ = true;

    int width_ = 1280;
    int height_ = 720;
    int format_ = 0;

    bool shouldRestart_ = false;
    bool running_ = false;

};

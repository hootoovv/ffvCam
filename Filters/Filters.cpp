#pragma warning(disable:4244)
#pragma warning(disable:4711)

#include <streams.h>
#include <stdio.h>
#include <olectl.h>
#include <dvdmedia.h>
#include <atlbase.h>

#include "common.h"
#include "properties.h"
#include "filters.h"
#include "videosource.h"

#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

using namespace std;
using namespace boost;

namespace fs = boost::filesystem;
namespace pt = boost::property_tree;

//////////////////////////////////////////////////////////////////////////
//  CVCam is the source filter which masquerades as a capture device
//////////////////////////////////////////////////////////////////////////
CUnknown * WINAPI CVCam::CreateInstance(LPUNKNOWN lpunk, HRESULT *phr)
{
    ASSERT(phr);
    CUnknown *punk = new CVCam(lpunk, phr);
    return punk;
}

CVCam::CVCam(LPUNKNOWN lpunk, HRESULT *phr) : 
    CSource(NAME(CAM_NAME), lpunk, CLSID_VirtualCam)
{
    ASSERT(phr);
    CAutoLock cAutoLock(&m_cStateLock);
    // Create the one and only output pin
    m_paStreams = (CSourceStream **) new CVCamStream*[1];
    m_paStreams[0] = new CVCamStream(phr, this, LCAM_NAME);
}

HRESULT CVCam::QueryInterface(REFIID riid, void **ppv)
{
    //Forward request for IAMStreamConfig & IKsPropertySet & IID_VirtualCam to the pin
    if(riid == _uuidof(IAMStreamConfig) || riid == _uuidof(IKsPropertySet) || riid == _uuidof(ISpecifyPropertyPages) || riid == IID_VirtualCam)
        return m_paStreams[0]->QueryInterface(riid, ppv);
    else
        return CSource::QueryInterface(riid, ppv);
}


DWORD WINAPI CamThreadProc(LPVOID lpParam)
{
    CVCamStream* pStream = (CVCamStream*)lpParam;

    bool resize = false;

    {
        CVideoSource src;
        int src_w = 0;
        int src_h = 0;
        int f = 0;
        bool rc = src.Check(pStream->m_Url, &src_w, &src_h, &f);

        if (pStream->m_currentWidth != src_w || pStream->m_currentHeight != src_h)
        {
            // anyway need resize
            resize = true;
        }
        else
        {
            // no need to resize, no matter how it configured.
            resize = false;
        }
    }

    CVideoSource source;

    source.Start(pStream->m_Url, pStream->m_Loop, pStream->m_Retry, pStream->m_Qsv, resize, pStream->m_currentWidth, pStream->m_currentHeight, pStream->m_Mode);
    source.SetFrame(pStream->m_frame, &pStream->m_cSharedFrame);

    while (!pStream->m_bStop)
    {
        // restart stopped stream
        if (source.shouldRestart())
        {
            if (source.shouldStop())
                source.Stop();

            // check retry interval, the interval is keep increasing from 1s to 15s max.
            // means: once disconnect, will retry connect quite frequently, like 1s, 2s, 3s, etc. but will get to slower and slower untill max 15s interval. 
            const std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
            const int Max_Retry = 15;
            int interval = 1000 * min(source.retry_, Max_Retry);
            std::chrono::milliseconds timeout(interval);

            if (now - source.lastRetry_ > timeout)
            {
                bool rc = source.Start(pStream->m_Url, pStream->m_Loop, pStream->m_Retry, pStream->m_Qsv, resize, pStream->m_currentWidth, pStream->m_currentHeight, pStream->m_Mode);

                if (rc)
                    source.retry_ = 0;
                else
                    source.retry_++;

                source.lastRetry_ = now;
            }
        }

        boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
    }

    source.Stop();

    return NOERROR;
}


//////////////////////////////////////////////////////////////////////////
// CVCamStream is the one and only output pin of CVCam which handles 
// all the stuff.
//////////////////////////////////////////////////////////////////////////
CVCamStream::CVCamStream(HRESULT *phr, CVCam *pParent, LPCWSTR pPinName) :
    CSourceStream(NAME(CAM_NAME),phr, pParent, pPinName), m_pParent(pParent)
{
    LoadProfile();

    // Set the default media type
    GetMediaType(&m_mt);

    DECLARE_PTR(VIDEOINFOHEADER, pvi, m_mt.Format());

    m_currentWidth = pvi->bmiHeader.biWidth;
    m_currentHeight = pvi->bmiHeader.biHeight;

    int frameSize = m_currentWidth * m_currentHeight * 3;// 12 / 8;
    m_frame = new BYTE[frameSize];
    ZeroMemory(m_frame, frameSize);
}

CVCamStream::~CVCamStream()
{
    delete[] m_frame;
} 

HRESULT CVCamStream::QueryInterface(REFIID riid, void **ppv)
{   
    // Standard OLE stuff
    if(riid == _uuidof(IAMStreamConfig))
        *ppv = (IAMStreamConfig*)this;
    else if(riid == _uuidof(IKsPropertySet))
        *ppv = (IKsPropertySet*)this;
    else if(riid == _uuidof(ISpecifyPropertyPages))
        *ppv = (ISpecifyPropertyPages*)this;
    else if(riid == IID_VirtualCam)
        *ppv = (IVirtualCam*)this;
    else
        return CSourceStream::QueryInterface(riid, ppv);

    AddRef();
    return S_OK;
}

HRESULT CVCamStream::GetPages(CAUUID* pPages)
{
    CheckPointer(pPages, E_POINTER);

    pPages->cElems = 1;
    pPages->pElems = (GUID*)CoTaskMemAlloc(sizeof(GUID));
    if (pPages->pElems == NULL)
    {
        return E_OUTOFMEMORY;
    }
    *(pPages->pElems) = CLSID_VirtualCamProp;
    return S_OK;
}

//////////////////////////////////////////////////////////////////////////
//  This is the routine where we create the data being output by the Virtual
//  Camera device.
//////////////////////////////////////////////////////////////////////////

HRESULT CVCamStream::FillBuffer(IMediaSample *pms)
{
    REFERENCE_TIME rtNow;
    
    REFERENCE_TIME avgFrameTime = ((VIDEOINFOHEADER*)m_mt.pbFormat)->AvgTimePerFrame;

    rtNow = m_rtLastTime;
    m_rtLastTime += avgFrameTime;
    pms->SetTime(&rtNow, &m_rtLastTime);
    pms->SetSyncPoint(TRUE);

    BYTE *pData;
    long lDataLen;
    pms->GetPointer(&pData);
    lDataLen = pms->GetSize();
    
    CAutoLock l(&m_cSharedFrame);
    memcpy(pData, m_frame, lDataLen);

    return NOERROR;
} // FillBuffer


//
// Notify
// Ignore quality management messages sent from the downstream filter
STDMETHODIMP CVCamStream::Notify(IBaseFilter * pSender, Quality q)
{
    return E_NOTIMPL;
} // Notify

//////////////////////////////////////////////////////////////////////////
// This is called when the output format has been negotiated
//////////////////////////////////////////////////////////////////////////
HRESULT CVCamStream::SetMediaType(const CMediaType *pmt)
{
    DECLARE_PTR(VIDEOINFOHEADER, pvi, pmt->Format());

    m_currentWidth = pvi->bmiHeader.biWidth;
    m_currentHeight = pvi->bmiHeader.biHeight;

    HRESULT hr = CSourceStream::SetMediaType(pmt);
    return hr;
}

// See Directshow help topic for IAMStreamConfig for details on this method
HRESULT CVCamStream::GetMediaType(CMediaType *pmt)
{
    LoadProfile();

    int w = 1280;
    int h = 720;

    if (m_Resize)
    {
        if (m_Index >= 8)
        {
            w = m_Width;
            h = m_Height;
        }
        else
        {
            w = m_listSize[m_Index].w;
            h = m_listSize[m_Index].h;
        }
    }
    else
    {
        if (m_sourceWidth != 0 && m_sourceHeight != 0)
        {
            w = m_sourceWidth;
            h = m_sourceHeight;
        }
        else
        {
            w = m_Width;
            h = m_Height;
        }
    }

    DECLARE_PTR(VIDEOINFOHEADER, pvi, pmt->AllocFormatBuffer(sizeof(VIDEOINFOHEADER)));
    ZeroMemory(pvi, sizeof(VIDEOINFOHEADER));

    pvi->bmiHeader.biCompression = MAKEFOURCC('I', '4', '2', '0');
    pvi->bmiHeader.biBitCount    = 12;
    pvi->bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    pvi->bmiHeader.biWidth       = w;
    pvi->bmiHeader.biHeight      = h;
    pvi->bmiHeader.biPlanes      = 3;
    pvi->bmiHeader.biSizeImage   = w * h * 3 / 2;
    pvi->bmiHeader.biClrImportant = 0;

    pvi->AvgTimePerFrame = 333333;

    SetRectEmpty(&(pvi->rcSource)); // we want the whole image area rendered.
    SetRectEmpty(&(pvi->rcTarget)); // no particular destination rectangle

    pmt->SetType(&MEDIATYPE_Video);
    pmt->SetFormatType(&FORMAT_VideoInfo);
    pmt->SetTemporalCompression(FALSE);

    // Work out the GUID for the subtype from the header info.
    const GUID SubTypeGUID = GetBitmapSubtype(&pvi->bmiHeader);
    pmt->SetSubtype(&SubTypeGUID);
    pmt->SetSampleSize(pvi->bmiHeader.biSizeImage);
    
    return NOERROR;

} // GetMediaType



// This method is called after the pins are connected to allocate buffers to stream data
HRESULT CVCamStream::DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pProperties)
{
    CAutoLock cAutoLock(m_pFilter->pStateLock());
    HRESULT hr = NOERROR;

    VIDEOINFOHEADER *pvi = (VIDEOINFOHEADER *) m_mt.Format();
    pProperties->cBuffers = 1;
    pProperties->cbBuffer = pvi->bmiHeader.biSizeImage;

    ALLOCATOR_PROPERTIES Actual;
    hr = pAlloc->SetProperties(pProperties,&Actual);

    if(FAILED(hr)) return hr;
    if(Actual.cbBuffer < pProperties->cbBuffer) return E_FAIL;

    return NOERROR;
} // DecideBufferSize

// Called when graph is run
HRESULT CVCamStream::OnThreadCreate()
{
    m_rtLastTime = 0;
    m_bStop = FALSE;

    m_hThread = CreateThread(NULL, 0, CamThreadProc, this, 0, NULL);

    return NOERROR;
} // OnThreadCreate

// Called when graph is stop running
HRESULT CVCamStream::OnThreadDestroy()
{
    m_bStop = TRUE;

    if (WaitForSingleObject(m_hThread, 500) == WAIT_TIMEOUT)
    {
        TerminateThread(m_hThread, NOERROR);
    }

    CloseHandle(m_hThread);

    return NOERROR;
} // OnThreadDestroy

//////////////////////////////////////////////////////////////////////////
//  IAMStreamConfig
//////////////////////////////////////////////////////////////////////////

HRESULT STDMETHODCALLTYPE CVCamStream::SetFormat(AM_MEDIA_TYPE *pmt)
{
    DECLARE_PTR(VIDEOINFOHEADER, pvi, m_mt.pbFormat);
    m_mt = *pmt;
    IPin* pin; 
    ConnectedTo(&pin);
    if(pin)
    {
        IFilterGraph *pGraph = m_pParent->GetGraph();
        pGraph->Reconnect(this);
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CVCamStream::GetFormat(AM_MEDIA_TYPE **ppmt)
{
    *ppmt = CreateMediaType(&m_mt);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CVCamStream::GetNumberOfCapabilities(int *piCount, int *piSize)
{
    *piCount = 1;
    *piSize = sizeof(VIDEO_STREAM_CONFIG_CAPS);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CVCamStream::GetStreamCaps(int iIndex, AM_MEDIA_TYPE **pmt, BYTE *pSCC)
{
    LoadProfile();

    int w = 1280;
    int h = 720;

    if (m_Resize)
    {
        if (m_Index >= 8)
        {
            w = m_Width;
            h = m_Height;
        }
        else
        {
            w = m_listSize[m_Index].w;
            h = m_listSize[m_Index].h;
        }
    }
    else
    {
        if (m_sourceWidth != 0 && m_sourceHeight != 0)
        {
            w = m_sourceWidth;
            h = m_sourceHeight;
        }
        else
        {
            w = m_Width;
            h = m_Height;
        }
    }

    *pmt = CreateMediaType(&m_mt);
    DECLARE_PTR(VIDEOINFOHEADER, pvi, (*pmt)->pbFormat);

    pvi->bmiHeader.biCompression = MAKEFOURCC('I', '4', '2', '0');
    pvi->bmiHeader.biBitCount    = 12;
    pvi->bmiHeader.biSize       = sizeof(BITMAPINFOHEADER);
    pvi->bmiHeader.biWidth      = w;
    pvi->bmiHeader.biHeight     = h;
    pvi->bmiHeader.biPlanes     = 3;
    pvi->bmiHeader.biSizeImage  = w * h * 3 / 2;
    pvi->bmiHeader.biClrImportant = 0;

    SetRectEmpty(&(pvi->rcSource)); // we want the whole image area rendered.
    SetRectEmpty(&(pvi->rcTarget)); // no particular destination rectangle

    (*pmt)->majortype = MEDIATYPE_Video;
    (*pmt)->subtype = MEDIASUBTYPE_IYUV;
    (*pmt)->formattype = FORMAT_VideoInfo;
    (*pmt)->bTemporalCompression = FALSE;
    (*pmt)->bFixedSizeSamples= TRUE;
    (*pmt)->lSampleSize = pvi->bmiHeader.biSizeImage;
    (*pmt)->cbFormat = sizeof(VIDEOINFOHEADER);
    
    DECLARE_PTR(VIDEO_STREAM_CONFIG_CAPS, pvscc, pSCC);
    
    pvscc->guid = FORMAT_VideoInfo;
    pvscc->VideoStandard = AnalogVideo_None;
    pvscc->InputSize.cx = w;
    pvscc->InputSize.cy = h;
    pvscc->MinCroppingSize.cx = 0;
    pvscc->MinCroppingSize.cy = 0;
    pvscc->MaxCroppingSize.cx = 0;
    pvscc->MaxCroppingSize.cy = 0;
    pvscc->CropGranularityX = 0;
    pvscc->CropGranularityY = 0;
    pvscc->CropAlignX = 0;
    pvscc->CropAlignY = 0;

    pvscc->MinOutputSize.cx = w;
    pvscc->MinOutputSize.cy = h;
    pvscc->MaxOutputSize.cx = w;
    pvscc->MaxOutputSize.cy = h;
    pvscc->OutputGranularityX = 0;
    pvscc->OutputGranularityY = 0;
    pvscc->StretchTapsX = 0;
    pvscc->StretchTapsY = 0;
    pvscc->ShrinkTapsX = 0;
    pvscc->ShrinkTapsY = 0;
    pvscc->MinFrameInterval = 166667;   //60 fps
    pvscc->MaxFrameInterval = 50000000; // 0.2 fps
    pvscc->MinBitsPerSecond = (pvi->bmiHeader.biSizeImage * 8) / 5;
    pvscc->MaxBitsPerSecond = pvi->bmiHeader.biSizeImage * 8 * 50;

    return S_OK;
}

//////////////////////////////////////////////////////////////////////////
// IKsPropertySet
//////////////////////////////////////////////////////////////////////////


HRESULT CVCamStream::Set(REFGUID guidPropSet, DWORD dwID, void *pInstanceData, 
                        DWORD cbInstanceData, void *pPropData, DWORD cbPropData)
{// Set: Cannot set any properties.
    return E_NOTIMPL;
}

// Get: Return the pin category (our only property). 
HRESULT CVCamStream::Get(
    REFGUID guidPropSet,   // Which property set.
    DWORD dwPropID,        // Which property in that set.
    void *pInstanceData,   // Instance data (ignore).
    DWORD cbInstanceData,  // Size of the instance data (ignore).
    void *pPropData,       // Buffer to receive the property data.
    DWORD cbPropData,      // Size of the buffer.
    DWORD *pcbReturned     // Return the size of the property.
)
{
    if (guidPropSet != AMPROPSETID_Pin)             return E_PROP_SET_UNSUPPORTED;
    if (dwPropID != AMPROPERTY_PIN_CATEGORY)        return E_PROP_ID_UNSUPPORTED;
    if (pPropData == NULL && pcbReturned == NULL)   return E_POINTER;
    
    if (pcbReturned) *pcbReturned = sizeof(GUID);
    if (pPropData == NULL)          return S_OK; // Caller just wants to know the size. 
    if (cbPropData < sizeof(GUID))  return E_UNEXPECTED;// The buffer is too small.
        
    *(GUID *)pPropData = PIN_CATEGORY_CAPTURE;
    return S_OK;
}

// QuerySupported: Query whether the pin supports the specified property.
HRESULT CVCamStream::QuerySupported(REFGUID guidPropSet, DWORD dwPropID, DWORD *pTypeSupport)
{
    if (guidPropSet != AMPROPSETID_Pin) return E_PROP_SET_UNSUPPORTED;
    if (dwPropID != AMPROPERTY_PIN_CATEGORY) return E_PROP_ID_UNSUPPORTED;
    // We support getting this property, but not setting it.
    if (pTypeSupport) *pTypeSupport = KSPROPERTY_SUPPORT_GET; 
    return S_OK;
}

HRESULT CVCamStream::get_IVirtualCamParams(BSTR* url, BOOL* resize, int* width, int* height, int* index, int* mode, BOOL* loop, BOOL* retry, BOOL* qsv)
{
    CAutoLock cAutolock(&m_camLock);
    CheckPointer(width, E_POINTER);
    CheckPointer(height, E_POINTER);

    TCHAR szUrl[1024];
    ZeroMemory(szUrl, sizeof(szUrl));
    MultiByteToWideChar(CP_UTF8, 0, m_Url.c_str(), m_Url.length(), szUrl, 1024);

    *url = szUrl;
    *resize = m_Resize;
    *width = m_Width;
    *height = m_Height;
    *index = m_Index;
    *mode = m_Mode;
    *loop = m_Loop;
    *retry = m_Retry;
    *qsv = m_Qsv;

    return NOERROR;
}

HRESULT CVCamStream::put_IVirtualCamParams(BSTR url, BOOL resize, int width, int height, int index, int mode, BOOL loop, BOOL retry, BOOL qsv)
{
    CAutoLock cAutolock(&m_camLock);

    char szUtf8[MAX_PATH];
    ZeroMemory(szUtf8, sizeof(szUtf8));

    WideCharToMultiByte(CP_UTF8, 0, url, -1, szUtf8, MAX_PATH, NULL, NULL);

    m_Url = szUtf8;
    m_Resize = resize;
    m_Width = width;
    m_Height = height;
    m_Index = index;
    m_Mode = mode;
    m_Loop = loop;
    m_Retry = retry;
    m_Qsv = qsv;

    string url_str = szUtf8;

    if (!url_str.empty() && !m_Resize)
    {
        CVideoSource src;
        int w = 1280;
        int h = 720;
        int f = 0;
        bool rc = src.Check(szUtf8, &w, &h, &f);

        if (rc)
        {
            m_Width = w;
            m_Height = h;
        }
    }

    SaveProfile();

    return NOERROR;
}

void CVCamStream::LoadProfile()
{
    TCHAR szPath[MAX_PATH];
    GetEnvironmentVariable(L"USERPROFILE", szPath, MAX_PATH);

    fs::path configPath(szPath);
    configPath.append(".ffvcam");
    configPath.append("ffvcam.ini");

    pt::ptree tree;

    if (fs::exists(configPath.string()))
    {
        pt::read_ini(configPath.string(), tree);
        m_Url = tree.get("Settings.Source", "");
        m_Resize = tree.get("Settings.Resize", false);
        m_Width = tree.get("Settings.Width", 1280);
        m_Height = tree.get("Settings.Height", 720);
        m_Index = tree.get("Settings.Index", 1);
        m_Mode = tree.get("Settings.Mode", 0);
        m_Loop = tree.get("Settings.Loop", true);
        m_Retry = tree.get("Settings.Retry", true);
        m_Qsv = tree.get("Settings.QsvDecode", true);
    }
    else
    {
        m_Url = "";
        m_Resize = false;
        m_Width = 1280;
        m_Height = 720;
        m_Index = 1;
        m_Mode = 0;
        m_Loop = true;
        m_Retry = true;
        m_Qsv = true;
    }

    m_listSize[0] = { 1920, 1080 };
    m_listSize[1] = { 1280, 720 };
    m_listSize[2] = { 960, 540 };
    m_listSize[3] = { 640, 360 };
    m_listSize[4] = { 1440, 1080 };
    m_listSize[5] = { 960, 720 };
    m_listSize[6] = { 640, 480 };
    m_listSize[7] = { 480, 360 };

    if (m_Url.empty())
    {
        m_sourceWidth = 0;
        m_sourceHeight = 0;
    } 
    else
    {
        CVideoSource src;
        int w = 1280;
        int h = 720;
        int f = 0;
        bool rc = src.Check(m_Url, &w, &h, &f);

        m_sourceWidth = w;
        m_sourceHeight = h;
    }

}

void CVCamStream::SaveProfile()
{
    TCHAR szPath[MAX_PATH];
    GetEnvironmentVariable(L"USERPROFILE", szPath, MAX_PATH);

    fs::path configPath(szPath);
    configPath.append(".ffvcam");
    if (!fs::exists(configPath.string()))
        fs::create_directory(configPath);

    configPath.append("ffvcam.ini");

    pt::ptree tree;
    tree.put("Settings.Source", m_Url);
    tree.put("Settings.Resize", m_Resize);
    tree.put("Settings.Width", m_Width);
    tree.put("Settings.Height", m_Height);
    tree.put("Settings.Index", m_Index);
    tree.put("Settings.Mode", m_Mode);
    tree.put("Settings.Loop", m_Loop);
    tree.put("Settings.Retry", m_Retry);
    tree.put("Settings.QsvDecode", m_Qsv);

    pt::write_ini(configPath.string(), tree);
}


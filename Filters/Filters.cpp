#pragma warning(disable:4244)
#pragma warning(disable:4711)

#include <streams.h>
#include <stdio.h>
#include <olectl.h>
#include <dvdmedia.h>
#include "common.h"
#include "properties.h"
#include "filters.h"

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


    while (!pStream->m_bStop)
    {
    }

    return NOERROR;
}


//////////////////////////////////////////////////////////////////////////
// CVCamStream is the one and only output pin of CVCam which handles 
// all the stuff.
//////////////////////////////////////////////////////////////////////////
CVCamStream::CVCamStream(HRESULT *phr, CVCam *pParent, LPCWSTR pPinName) :
    CSourceStream(NAME(CAM_NAME),phr, pParent, pPinName), m_pParent(pParent)
{
    // Set the default media type
    GetMediaType(0, &m_mt);

    LoadProfile();
}

CVCamStream::~CVCamStream()
{
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
    for(int i = 0; i < lDataLen; ++i)
        pData[i] = rand();

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
    HRESULT hr = CSourceStream::SetMediaType(pmt);
    return hr;
}

// See Directshow help topic for IAMStreamConfig for details on this method
HRESULT CVCamStream::GetMediaType(int iPosition, CMediaType *pmt)
{
    if(iPosition < 0) return E_INVALIDARG;
    if(iPosition > 8) return VFW_S_NO_MORE_ITEMS;

    DECLARE_PTR(VIDEOINFOHEADER, pvi, pmt->AllocFormatBuffer(sizeof(VIDEOINFOHEADER)));
    ZeroMemory(pvi, sizeof(VIDEOINFOHEADER));

    pvi->bmiHeader.biCompression = MAKEFOURCC('I', '4', '2', '0');
    pvi->bmiHeader.biBitCount    = 12;
    pvi->bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    pvi->bmiHeader.biWidth       = m_listSize[iPosition].w;
    pvi->bmiHeader.biHeight      = m_listSize[iPosition].h;
    pvi->bmiHeader.biPlanes      = 3;
    pvi->bmiHeader.biSizeImage   = m_listSize[iPosition].w * m_listSize[iPosition].h * 3 / 2;
    pvi->bmiHeader.biClrImportant = 0;

    pvi->AvgTimePerFrame = 1000000;

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

// This method is called to see if a given output format is supported
HRESULT CVCamStream::CheckMediaType(const CMediaType *pMediaType)
{
    VIDEOINFOHEADER *pvi = (VIDEOINFOHEADER *)(pMediaType->Format());
    if(*pMediaType != m_mt) 
        return E_INVALIDARG;
    return S_OK;
} // CheckMediaType

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
    *piCount = 9;
    *piSize = sizeof(VIDEO_STREAM_CONFIG_CAPS);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CVCamStream::GetStreamCaps(int iIndex, AM_MEDIA_TYPE **pmt, BYTE *pSCC)
{
    *pmt = CreateMediaType(&m_mt);
    DECLARE_PTR(VIDEOINFOHEADER, pvi, (*pmt)->pbFormat);

    if (iIndex > 8)
        iIndex = 0;

    pvi->bmiHeader.biCompression = MAKEFOURCC('I', '4', '2', '0');
    pvi->bmiHeader.biBitCount    = 12;
    pvi->bmiHeader.biSize       = sizeof(BITMAPINFOHEADER);
    pvi->bmiHeader.biWidth      = m_listSize[iIndex].w;
    pvi->bmiHeader.biHeight     = m_listSize[iIndex].h;
    pvi->bmiHeader.biPlanes     = 3;
    pvi->bmiHeader.biSizeImage  = m_listSize[iIndex].w * m_listSize[iIndex].h * 3 / 2;
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
    pvscc->InputSize.cx = m_listSize[iIndex].w;
    pvscc->InputSize.cy = m_listSize[iIndex].h;
    pvscc->MinCroppingSize.cx = 0;
    pvscc->MinCroppingSize.cy = 0;
    pvscc->MaxCroppingSize.cx = 0;
    pvscc->MaxCroppingSize.cy = 0;
    pvscc->CropGranularityX = 0;
    pvscc->CropGranularityY = 0;
    pvscc->CropAlignX = 0;
    pvscc->CropAlignY = 0;

    pvscc->MinOutputSize.cx = m_listSize[iIndex].w;
    pvscc->MinOutputSize.cy = m_listSize[iIndex].h;
    pvscc->MaxOutputSize.cx = m_listSize[iIndex].w;
    pvscc->MaxOutputSize.cy = m_listSize[iIndex].h;
    pvscc->OutputGranularityX = 0;
    pvscc->OutputGranularityY = 0;
    pvscc->StretchTapsX = 0;
    pvscc->StretchTapsY = 0;
    pvscc->ShrinkTapsX = 0;
    pvscc->ShrinkTapsY = 0;
    pvscc->MinFrameInterval = 200000;   //50 fps
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
STDMETHODIMP get_IVirtualCamParams(BSTR* url, BOOL* resize, int* width, int* height, int* index, int* mode);
STDMETHODIMP put_IVirtualCamParams(BSTR url, BOOL resize, int width, int height, int index, int mode);

HRESULT CVCamStream::get_IVirtualCamParams(BSTR* url, BOOL* resize, int* width, int* height, int* index, int* mode)
{
    CAutoLock cAutolock(&m_camLock);
    CheckPointer(width, E_POINTER);
    CheckPointer(height, E_POINTER);

    *url = m_Url.Copy();
    *resize = m_Resize;
    *width = m_Width;
    *height = m_Height;
    *index = m_Index;
    *mode = m_Mode;

    return NOERROR;
}

HRESULT CVCamStream::put_IVirtualCamParams(BSTR url, BOOL resize, int width, int height, int index, int mode)
{
    CAutoLock cAutolock(&m_camLock);

    m_Url = url;
    m_Resize = resize;
    m_Width = width;
    m_Height = height;
    m_Index = index;
    m_Mode = mode;

    SaveProfile();

    return NOERROR;
}

void CVCamStream::LoadProfile()
{
    m_Url = L"d:\\temp\\1.mpg";
    m_Resize = TRUE;
    m_Width = 1280;
    m_Height = 720;
    m_Index = 1;
    m_Mode = 0;

    m_listSize[0] = { m_Width, m_Height };

    m_listSize[1] = { 1920, 1080 };
    m_listSize[2] = { 1280, 720 };
    m_listSize[3] = { 960, 540 };
    m_listSize[4] = { 640, 360 };

    m_listSize[5] = { 1440, 1080 };
    m_listSize[6] = { 960, 720 };
    m_listSize[7] = { 640, 480 };
    m_listSize[8] = { 480, 360 };

}

void CVCamStream::SaveProfile()
{

}


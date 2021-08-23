#pragma once

#include <string>

#define DECLARE_PTR(type, ptr, expr) type* ptr = (type*)(expr);

struct Video_Size
{
    int w;
    int h;
};

class CVCamStream;
class CVCam : public CSource
{
public:
    //////////////////////////////////////////////////////////////////////////
    //  IUnknown
    //////////////////////////////////////////////////////////////////////////
    static CUnknown * WINAPI CreateInstance(LPUNKNOWN lpunk, HRESULT *phr);
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv);

    IFilterGraph *GetGraph() {return m_pGraph;}

private:
    CVCam(LPUNKNOWN lpunk, HRESULT *phr);
};

class CVCamStream : public CSourceStream, public IAMStreamConfig, public IKsPropertySet, public ISpecifyPropertyPages, public IVirtualCam
{
public:

    //////////////////////////////////////////////////////////////////////////
    //  IUnknown
    //////////////////////////////////////////////////////////////////////////
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv);
    STDMETHODIMP_(ULONG) AddRef() { return GetOwner()->AddRef(); }                                                          \
    STDMETHODIMP_(ULONG) Release() { return GetOwner()->Release(); }

    //////////////////////////////////////////////////////////////////////////
    //  IQualityControl
    //////////////////////////////////////////////////////////////////////////
    STDMETHODIMP Notify(IBaseFilter * pSender, Quality q);

    //////////////////////////////////////////////////////////////////////////
    //  IAMStreamConfig
    //////////////////////////////////////////////////////////////////////////
    HRESULT STDMETHODCALLTYPE SetFormat(AM_MEDIA_TYPE *pmt);
    HRESULT STDMETHODCALLTYPE GetFormat(AM_MEDIA_TYPE **ppmt);
    HRESULT STDMETHODCALLTYPE GetNumberOfCapabilities(int *piCount, int *piSize);
    HRESULT STDMETHODCALLTYPE GetStreamCaps(int iIndex, AM_MEDIA_TYPE **pmt, BYTE *pSCC);

    //////////////////////////////////////////////////////////////////////////
    //  IKsPropertySet
    //////////////////////////////////////////////////////////////////////////
    HRESULT STDMETHODCALLTYPE Set(REFGUID guidPropSet, DWORD dwID, void *pInstanceData, DWORD cbInstanceData, void *pPropData, DWORD cbPropData);
    HRESULT STDMETHODCALLTYPE Get(REFGUID guidPropSet, DWORD dwPropID, void *pInstanceData,DWORD cbInstanceData, void *pPropData, DWORD cbPropData, DWORD *pcbReturned);
    HRESULT STDMETHODCALLTYPE QuerySupported(REFGUID guidPropSet, DWORD dwPropID, DWORD *pTypeSupport);
    
    // ISpecifyPropertyPages interface
    STDMETHODIMP GetPages(CAUUID* pPages);

    //////////////////////////////////////////////////////////////////////////
    //  CSourceStream
    //////////////////////////////////////////////////////////////////////////
    CVCamStream(HRESULT *phr, CVCam *pParent, LPCWSTR pPinName);
    ~CVCamStream();

    HRESULT FillBuffer(IMediaSample *pms);
    HRESULT DecideBufferSize(IMemAllocator *pIMemAlloc, ALLOCATOR_PROPERTIES *pProperties);
    HRESULT GetMediaType(CMediaType *pmt);
    HRESULT SetMediaType(const CMediaType *pmt);
    HRESULT OnThreadCreate(void);
    HRESULT OnThreadDestroy(void);

    // These implement the custom IIScreenCam interface
    STDMETHODIMP get_IVirtualCamParams(BSTR* url, BOOL* resize, int* width, int* height, int* index, int* mode, BOOL* loop, BOOL* retry, BOOL* qsv);
    STDMETHODIMP put_IVirtualCamParams(BSTR url, BOOL resize, int width, int height, int index, int mode, BOOL loop, BOOL retry, BOOL qsv);

    void LoadProfile();
    void SaveProfile();

    CCritSec    m_camLock;
    BOOL    m_bStop;

    std::string		m_Url;
    BOOL			m_Resize;
    int				m_Width;
    int				m_Height;
    int				m_Index;
    int				m_Mode;
    BOOL            m_Loop;
    BOOL            m_Retry;
    BOOL            m_Qsv;

	BYTE*   m_frame;

    int m_currentWidth;
    int m_currentHeight;
    int m_sourceWidth;
    int m_sourceHeight;

    CCritSec m_cSharedFrame;

private:
    CVCam *m_pParent;
    REFERENCE_TIME m_rtLastTime;
    HBITMAP m_hLogoBmp;
    IReferenceClock *m_pClock;

    HANDLE m_hThread;

    Video_Size m_listSize[9];
};



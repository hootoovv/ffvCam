#pragma once

#define CAM_NAME "ffvCam"
#define LCAM_NAME L"ffvCam"
#define LCAM_PROP_NAME L"ffvCam Properties"

#ifdef __cplusplus
extern "C" {
#endif

    // {8E14549A-DB61-4309-AFA1-5578E927E999}
    DEFINE_GUID(CLSID_VirtualCam,
        0x8e14549a, 0xdb61, 0x4309, 0xaf, 0xa1, 0x55, 0x78, 0xe9, 0x27, 0xe9, 0x99);

    // {782E44B8-0E1D-419D-BDEC-595881903A1B}
    DEFINE_GUID(CLSID_VirtualCamProp,
        0x782e44b8, 0xe1d, 0x419d, 0xbd, 0xec, 0x59, 0x58, 0x81, 0x90, 0x3a, 0x1b);

    // {D197CC58-0C23-4AFF-88A3-57B6A94E2B37}
    DEFINE_GUID(IID_VirtualCam,
        0xd197cc58, 0xc23, 0x4aff, 0x88, 0xa3, 0x57, 0xb6, 0xa9, 0x4e, 0x2b, 0x37);

    DECLARE_INTERFACE_(IVirtualCam, IUnknown)
    {
        STDMETHOD(get_IVirtualCamParams) (THIS_
            BSTR* url,
            BOOL* resize,
            int* width,
            int* height,
            int* index,
            int* mode
            ) PURE;

        STDMETHOD(put_IVirtualCamParams) (THIS_
            BSTR url,
            BOOL resize,
            int width,
            int height,
            int index,
            int mode
            ) PURE;
    };

#ifdef __cplusplus
}
#endif
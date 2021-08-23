#include <windows.h>
#include <windowsx.h>
#include <streams.h>
#include <commctrl.h>
#include <olectl.h>
#include <memory.h>
#include <stdlib.h>
#include <stdio.h>
#include <tchar.h>
#include <strsafe.h>
#include <shlwapi.h>
#include "resource.h"
#include "common.h"
#include "Properties.h"
#include "Filters.h"
#include "videosource.h"

CUnknown* CVCamProp::CreateInstance(LPUNKNOWN lpunk, HRESULT* phr)
{
    CUnknown* punk = new CVCamProp(lpunk);

    if (punk == NULL) {
        if (phr)
            *phr = E_OUTOFMEMORY;
    }

    return punk;

} // CreateInstance

CVCamProp::CVCamProp(IUnknown *pUnk)
:CBasePropertyPage(LCAM_PROP_NAME, pUnk, IDD_PROPPAGE, IDS_APP_TITLE),
    m_vCam(NULL),
    m_bIsInitialized(FALSE)
{
}

CVCamProp::~CVCamProp()
{
}
HRESULT CVCamProp::OnConnect(IUnknown *pUnknown)
{
    CheckPointer(pUnknown, E_POINTER);

    HRESULT hr = pUnknown->QueryInterface(IID_VirtualCam, (void**)&m_vCam);
    if (FAILED(hr)) {
        return E_NOINTERFACE;
    }

    CheckPointer(m_vCam, E_FAIL);
	BSTR url;
    m_vCam->get_IVirtualCamParams(&url, &m_Resize, &m_Width, &m_Height, &m_Index, &m_Mode);
	m_Url = url;

    m_bIsInitialized = FALSE;
    return NOERROR;
}

HRESULT CVCamProp::OnDisconnect()
{
    if (m_vCam)
    {
        m_vCam->Release();
        m_vCam = NULL;
    }
    return NOERROR;
}

HRESULT CVCamProp::OnActivate()
{
	m_bIsInitialized = TRUE;

	return NOERROR;
}

HRESULT CVCamProp::OnDeactivate(void)
{
    m_bIsInitialized = FALSE;

   return NOERROR;

}

HRESULT CVCamProp::OnApplyChanges()
{
    GetControlValues();

	CheckPointer(m_vCam, E_POINTER);
	BSTR url = m_Url.Copy();
    m_vCam->put_IVirtualCamParams(url, m_Resize, m_Width, m_Height, m_Index, m_Mode);

    return NOERROR;
}

INT_PTR CVCamProp::OnReceiveMessage(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	switch (uMsg)  
	{  
	case WM_INITDIALOG:  
		{  
			INITCOMMONCONTROLSEX icc;
			icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
			icc.dwICC = ICC_BAR_CLASSES;
			if (InitCommonControlsEx(&icc) == FALSE)
			{
				return E_FAIL;
			}

			SetDlgItemText(m_Dlg, IDC_EDIT_URL, m_Url);

			int index = 0;
			
			if (m_Resize)
				index = 1;

			CheckRadioButton(m_Dlg, IDC_RADIO_SOURCE, IDC_RADIO_RESIZE, IDC_RADIO_SOURCE+index);

			CheckRadioButton(m_Dlg, IDC_RADIO_1920x1080, IDC_RADIO_Custmization, IDC_RADIO_1920x1080+m_Index);

			TCHAR v[256];
			(void)StringCchPrintf(v, NUMELMS(v), TEXT("%d\0"), m_Width);
			SetDlgItemText(m_Dlg, IDC_EDIT_WIDTH, v);

			(void)StringCchPrintf(v, NUMELMS(v), TEXT("%d\0"), m_Height);
			SetDlgItemText(m_Dlg, IDC_EDIT_HEIGHT, v);

			CheckRadioButton(m_Dlg, IDC_RADIO_FIT, IDC_RADIO_STRETCH, IDC_RADIO_FIT+m_Mode);

			EnableWindow(GetDlgItem(m_Dlg, IDC_RADIO_1920x1080), m_Resize);
			EnableWindow(GetDlgItem(m_Dlg, IDC_RADIO_1280x720), m_Resize);
			EnableWindow(GetDlgItem(m_Dlg, IDC_RADIO_960x540), m_Resize);
			EnableWindow(GetDlgItem(m_Dlg, IDC_RADIO_640x360), m_Resize);
			EnableWindow(GetDlgItem(m_Dlg, IDC_RADIO_1440x1080), m_Resize);
			EnableWindow(GetDlgItem(m_Dlg, IDC_RADIO_960x720), m_Resize);
			EnableWindow(GetDlgItem(m_Dlg, IDC_RADIO_640x480), m_Resize);
			EnableWindow(GetDlgItem(m_Dlg, IDC_RADIO_430x360), m_Resize);
			EnableWindow(GetDlgItem(m_Dlg, IDC_RADIO_Custmization), m_Resize);
			EnableWindow(GetDlgItem(m_Dlg, IDC_RADIO_FIT), m_Resize);
			EnableWindow(GetDlgItem(m_Dlg, IDC_RADIO_CLIP), m_Resize);
			EnableWindow(GetDlgItem(m_Dlg, IDC_RADIO_STRETCH), m_Resize);

			EnableWindow(GetDlgItem(m_Dlg, IDC_EDIT_WIDTH), m_Resize && m_Index == 8);
			EnableWindow(GetDlgItem(m_Dlg, IDC_EDIT_HEIGHT), m_Resize && m_Index == 8);

			return TRUE ;
		}	
	case WM_DESTROY :  
		{  
			break ;  
		}
	case WM_COMMAND:  
		{  
			TCHAR szFile[MAX_PATH];

			if(LOWORD(wParam) == IDC_BUTTON_BROWSE)
			{
				// 打开文件打开对话框，如果选中文件，则NewGame
				OPENFILENAME ofn;      // 公共对话框结构。

				// 初始化选择文件对话框。
				ZeroMemory(&ofn, sizeof(ofn));
				ofn.lStructSize = sizeof(ofn);
				ofn.hwndOwner = m_hwnd;
				ofn.lpstrFile = szFile;
				//
				//
				ofn.lpstrFile[0] = NULL;
				ofn.nMaxFile = sizeof(szFile);
				ofn.lpstrFilter = L"video file\0*.mp4; *.m4v; *.mov; *.flv; *.f4v; *.wmv; *.asf; *.avi; *.rm; *.rmvb; *.dat; *.mpg; *.mpeg; *.vob; *.mkv; *.3gp; *.vp6; *.ts; *.tp; *.avs; *.ogm; *.ifo; *nsv; *.m2ts\0all files\0*.*\0";
				ofn.nFilterIndex = 1;
				ofn.lpstrFileTitle = NULL;
				ofn.nMaxFileTitle = 0;
				ofn.lpstrInitialDir = NULL;
				ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

				// 显示打开选择文件对话框。
				if (GetOpenFileName(&ofn))
				{
					SetDlgItemText(m_Dlg, IDC_EDIT_URL, szFile);
				}  
			}
			else if (LOWORD(wParam) == IDC_BUTTON_TEST)
			{
				TCHAR v[1024];
				GetDlgItemText(m_Dlg, IDC_EDIT_URL, v, 1024);

				char szUtf8[MAX_PATH];
				ZeroMemory(szUtf8, sizeof(szUtf8));

				WideCharToMultiByte(CP_UTF8, 0, v, -1, szUtf8, MAX_PATH, NULL, NULL);
				string url = szUtf8;

				if (!url.empty())
				{
					CVideoSource src;
					int w = 1280;
					int h = 720;
					int f = 0;
					bool rc = src.Check(szUtf8, &w, &h, &f);

					if (rc)
					{
						TCHAR msg[1024];

						(void)StringCchPrintf(msg, NUMELMS(msg), TEXT("Video Size: %dx%d\0"), w, h);
						SetDlgItemText(m_Dlg, IDC_STATUS, msg);
					}
					else
					{
						SetDlgItemText(m_Dlg, IDC_STATUS, L"Failed to get video size.");
					}
				}
			}
			else if (LOWORD(wParam) == IDC_RADIO_SOURCE)
			{
				EnableWindow(GetDlgItem(m_Dlg, IDC_RADIO_1920x1080), FALSE);
				EnableWindow(GetDlgItem(m_Dlg, IDC_RADIO_1280x720), FALSE);
				EnableWindow(GetDlgItem(m_Dlg, IDC_RADIO_960x540), FALSE);
				EnableWindow(GetDlgItem(m_Dlg, IDC_RADIO_640x360), FALSE);
				EnableWindow(GetDlgItem(m_Dlg, IDC_RADIO_1440x1080), FALSE);
				EnableWindow(GetDlgItem(m_Dlg, IDC_RADIO_960x720), FALSE);
				EnableWindow(GetDlgItem(m_Dlg, IDC_RADIO_640x480), FALSE);
				EnableWindow(GetDlgItem(m_Dlg, IDC_RADIO_430x360), FALSE);
				EnableWindow(GetDlgItem(m_Dlg, IDC_RADIO_Custmization), FALSE);
				EnableWindow(GetDlgItem(m_Dlg, IDC_EDIT_WIDTH), FALSE);
				EnableWindow(GetDlgItem(m_Dlg, IDC_EDIT_HEIGHT), FALSE);
				EnableWindow(GetDlgItem(m_Dlg, IDC_RADIO_FIT), FALSE);
				EnableWindow(GetDlgItem(m_Dlg, IDC_RADIO_CLIP), FALSE);
				EnableWindow(GetDlgItem(m_Dlg, IDC_RADIO_STRETCH), FALSE);
			}
			else if (LOWORD(wParam) == IDC_RADIO_RESIZE)
			{
				EnableWindow(GetDlgItem(m_Dlg, IDC_RADIO_1920x1080), TRUE);
				EnableWindow(GetDlgItem(m_Dlg, IDC_RADIO_1280x720), TRUE);
				EnableWindow(GetDlgItem(m_Dlg, IDC_RADIO_960x540), TRUE);
				EnableWindow(GetDlgItem(m_Dlg, IDC_RADIO_640x360), TRUE);
				EnableWindow(GetDlgItem(m_Dlg, IDC_RADIO_1440x1080), TRUE);
				EnableWindow(GetDlgItem(m_Dlg, IDC_RADIO_960x720), TRUE);
				EnableWindow(GetDlgItem(m_Dlg, IDC_RADIO_640x480), TRUE);
				EnableWindow(GetDlgItem(m_Dlg, IDC_RADIO_430x360), TRUE);
				EnableWindow(GetDlgItem(m_Dlg, IDC_RADIO_Custmization), TRUE);
				EnableWindow(GetDlgItem(m_Dlg, IDC_RADIO_FIT), TRUE);
				EnableWindow(GetDlgItem(m_Dlg, IDC_RADIO_CLIP), TRUE);
				EnableWindow(GetDlgItem(m_Dlg, IDC_RADIO_STRETCH), TRUE);

				if (IsDlgButtonChecked(m_Dlg, IDC_RADIO_Custmization))
				{
					EnableWindow(GetDlgItem(m_Dlg, IDC_EDIT_WIDTH), TRUE);
					EnableWindow(GetDlgItem(m_Dlg, IDC_EDIT_HEIGHT), TRUE);
				}
				else
				{
					EnableWindow(GetDlgItem(m_Dlg, IDC_EDIT_WIDTH), FALSE);
					EnableWindow(GetDlgItem(m_Dlg, IDC_EDIT_HEIGHT), FALSE);
				}
			}
			else if (LOWORD(wParam) == IDC_RADIO_Custmization)
			{
				EnableWindow(GetDlgItem(m_Dlg, IDC_EDIT_WIDTH), TRUE);
				EnableWindow(GetDlgItem(m_Dlg, IDC_EDIT_HEIGHT), TRUE);
			}
			else if (LOWORD(wParam) >= IDC_RADIO_1920x1080 && LOWORD(wParam) < IDC_RADIO_Custmization)
			{
				EnableWindow(GetDlgItem(m_Dlg, IDC_EDIT_WIDTH), FALSE);
				EnableWindow(GetDlgItem(m_Dlg, IDC_EDIT_HEIGHT), FALSE);
			}


	        if (m_bIsInitialized)
	        {
	            m_bDirty = TRUE;
	            if (m_pPageSite)
	            {
	                m_pPageSite->OnStatusChange(PROPPAGESTATUS_DIRTY);
	            }
			}

			return (LRESULT)1;
		}  
	}

	return CBasePropertyPage::OnReceiveMessage (hwnd,uMsg,wParam,lParam);
}
void CVCamProp::GetControlValues()
{
	if (IsDlgButtonChecked(m_Dlg, IDC_RADIO_RESIZE))
		m_Resize = TRUE;
	else
		m_Resize = FALSE;

	if (IsDlgButtonChecked(m_Dlg, IDC_RADIO_1920x1080))
		m_Index = 0;
	else if (IsDlgButtonChecked(m_Dlg, IDC_RADIO_1280x720))
		m_Index = 1;
	else if (IsDlgButtonChecked(m_Dlg, IDC_RADIO_960x540))
		m_Index = 2;
	else if (IsDlgButtonChecked(m_Dlg, IDC_RADIO_640x360))
		m_Index = 3;
	else if (IsDlgButtonChecked(m_Dlg, IDC_RADIO_1440x1080))
		m_Index = 4;
	else if (IsDlgButtonChecked(m_Dlg, IDC_RADIO_960x720))
		m_Index = 5;
	else if (IsDlgButtonChecked(m_Dlg, IDC_RADIO_640x480))
		m_Index = 6;
	else if (IsDlgButtonChecked(m_Dlg, IDC_RADIO_430x360))
		m_Index = 7;
	else
		m_Index = 8;

	if (IsDlgButtonChecked(m_Dlg, IDC_RADIO_FIT))
		m_Mode = 0;
	else if (IsDlgButtonChecked(m_Dlg, IDC_RADIO_CLIP))
		m_Mode = 1;
	else if (IsDlgButtonChecked(m_Dlg, IDC_RADIO_STRETCH))
		m_Mode = 2;

	TCHAR v[1024];
	GetDlgItemText(m_Dlg, IDC_EDIT_URL, v, 1024);

	m_Url = v;

	GetDlgItemText(m_Dlg, IDC_EDIT_WIDTH, v, 1024);

	char szANSI[STR_MAX_LENGTH];

	int rc = WideCharToMultiByte(CP_ACP, 0, v, -1, szANSI, STR_MAX_LENGTH, NULL, NULL);
	m_Width = atoi(szANSI);

	GetDlgItemText(m_Dlg, IDC_EDIT_HEIGHT, v, 1024);

	rc = WideCharToMultiByte(CP_ACP, 0, v, -1, szANSI, STR_MAX_LENGTH, NULL, NULL);
	m_Height = atoi(szANSI);
}

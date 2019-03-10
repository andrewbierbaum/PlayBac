// PlayBacDlg.cpp : implementation file
//

#include "stdafx.h"

#include "PlayBac.h"
#include "PlayBacDlg.h"
#include "FrameInfo.h"

#include "DShow.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

enum PLAYSTATE {Paused, Running, Init, Nothing, Stopped};
HACCEL m_hAccel;


//functions
void CenterVideo(void);

//usefull variables
RECT rc;
DWORD     g_dwGraphRegister=0;
PLAYSTATE g_psCurrent=Nothing;
FrameInfo frameinfoarray[250000];   //grouped as units of 25
int initialframe=-1;
REFERENCE_TIME currentposition;
REFERENCE_TIME duration;
long nativewidth;
long nativeheight;
int xcurrent;
int ycurrent;
int currentdatapoint=0;
int sizewarning=0;

//directshow plugins
IGraphBuilder   *pGB=NULL;
IMediaSeeking   *pMS=NULL;
IMediaControl   *pMC=NULL;
IMediaEventEx   *pME=NULL;
IVideoWindow    *pVW=NULL;
IMediaPosition  *pMP=NULL;
IBasicVideo     *pBV=NULL;

// CPlayBacDlg dialog

CPlayBacDlg::CPlayBacDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CPlayBacDlg::IDD, pParent)
	, m_time1(0)
	, m_Duration(0)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CPlayBacDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_FRAME, m_Screen);
	DDX_Control(pDX, IDC_FILENAME, m_filename);
	DDX_Text(pDX, IDC_EDIT3, m_time1);
	DDX_Text(pDX, IDC_EDIT12, m_Duration);
}


BEGIN_MESSAGE_MAP(CPlayBacDlg, CDialog)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDC_PLAY, OnBnClickedPlay)
	ON_COMMAND(ID_FILE_OPEN138, OnFileOpen138)
	ON_COMMAND(ID_CONTROLS_STEP, OnControlsStep)
	ON_WM_TIMER()
	ON_COMMAND(ID_CONTROLS_TAKEDATAPOINT, OnControlsTakedatapoint)
	ON_COMMAND(ID_FILE_EXPORTDATA, OnFileExportdata)
	ON_BN_CLICKED(IDC_BACKWARD, OnBnClickedBackward)
	ON_BN_CLICKED(IDC_FORWARD, OnBnClickedForward)
	ON_BN_CLICKED(IDC_BACKWARD10, OnBnClickedBackward10)
	ON_BN_CLICKED(IDC_FORWARD10, OnBnClickedForward10)
	ON_BN_CLICKED(IDC_POST, OnBnClickedPost)
	ON_COMMAND(ID_CONTROLS_STEPBACK148, OnControlsStepback148)
	ON_BN_CLICKED(IDC_RESIZE, OnBnClickedResize)
	ON_BN_CLICKED(IDC_F100, OnBnClickedF100)
	ON_BN_CLICKED(IDC_F1000, OnBnClickedF1000)
	ON_BN_CLICKED(IDC_B100, OnBnClickedB100)
	ON_BN_CLICKED(IDC_B1000, OnBnClickedB1000)
	ON_BN_CLICKED(IDC_BUTTON3, OnBnClickedButton3)
	ON_BN_CLICKED(IDC_SETDATAPOINTBUTTON, OnBnClickedSetdatapointbutton)
END_MESSAGE_MAP()


// CPlayBacDlg message handlers

BOOL CPlayBacDlg::OnInitDialog()
{
	// Load accelerator keys...
	m_hAccel = ::LoadAccelerators(AfxGetInstanceHandle(),
                              MAKEINTRESOURCE(IDR_MY_ACCELERATOR));
	ASSERT(m_hAccel);

	
	CDialog::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// TODO: Add extra initialization here
	
	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CPlayBacDlg::OnPaint() 
{

	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}

	SetTimer(IDC_TIMER, 50, NULL);


}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CPlayBacDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}
/////////////////////////////////////////////////////////////////////
// direct show to button interface

void CPlayBacDlg::SetDefaults(void)
{
	pGB   = NULL;
	pMC   = NULL;
	pME   = NULL;
	pVW   = NULL;
	pMS   = NULL;
	pMP   = NULL;
	pBV   = NULL;
\


	HRESULT hr = CoInitialize(NULL);
	if (FAILED(hr))
	{	
		printf("ERROR - Could not initialize COM library");
		return;
	}

	CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void **)&pGB);
}

// Removes a filter graph from the Running Object Table
void RemoveGraphFromRot(DWORD pdwRegister)
{
    IRunningObjectTable *pROT;

    if (SUCCEEDED(GetRunningObjectTable(0, &pROT))) 
    {
        pROT->Revoke(pdwRegister);
        pROT->Release();
    }
}


HRESULT CPlayBacDlg::InitDirectShow(void)
{	
    // Get interfaces
    CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC, IID_IGraphBuilder, (void **)&pGB);
	pGB->QueryInterface(IID_IMediaControl,  (void **)&pMC);
	pGB->QueryInterface(IID_IMediaSeeking,  (void **)&pMS);
	pGB->QueryInterface(IID_IVideoWindow,   (void **)&pVW);	
	pGB->QueryInterface(IID_IMediaEventEx,  (void **)&pME);
	pGB->QueryInterface(IID_IMediaPosition, (void **)&pMP);
	pGB->QueryInterface(IID_IBasicVideo,    (void **)&pBV);	

    return S_OK;

}

HRESULT CPlayBacDlg::FreeDirectShow(void)
{
    HRESULT hr=S_OK;

	if(pMC) //used to keep initial conditions from messing up the program
	{
	pMC->Stop();
	g_psCurrent = Init;
	}

    // Disable event callbacks
    if (pME)
        hr = pME->SetNotifyWindow((OAHWND)NULL, 0, 0);

    // Hide video window and remove owner.  This is not necessary here,
    // since we are about to destroy the filter graph, but it is included
    // for demonstration purposes.  Remember to hide the video window and
    // clear its owner when destroying a window that plays video.
    if(pVW)
    {
        hr = pVW->put_Visible(OAFALSE);
        hr = pVW->put_Owner(NULL);
    }

#ifdef DEBUG
    RemoveGraphFromRot(m_dwRegister);
#endif
	
	if(pMC)
	{
	pGB->Release();
    pMC->Release();
    pME->Release();
    pVW->Release();
    pMS->Release();
    pMP->Release();
	pBV->Release();
	}
    return hr;
}

void CPlayBacDlg::ResetDirectShow(void)
{
    // Destroy the current filter graph its filters.
    FreeDirectShow();
	
    // Reinitialize graph builder and query for interfaces
    InitDirectShow();
}


HRESULT AddGraphToRot(IUnknown *pUnkGraph, DWORD *pdwRegister) 
{
    IMoniker * pMoniker;
    IRunningObjectTable *pROT;
    WCHAR wsz[128];
    HRESULT hr;

    if (!pUnkGraph || !pdwRegister)
        return E_POINTER;

    if (FAILED(GetRunningObjectTable(0, &pROT)))
        return E_FAIL;

    wsprintfW(wsz, L"FilterGraph %08x pid %08x\0", (DWORD_PTR)pUnkGraph, 
              GetCurrentProcessId());

    hr = CreateItemMoniker(L"!", wsz, &pMoniker);
    if (SUCCEEDED(hr)) 
    {
        // Use the ROTFLAGS_REGISTRATIONKEEPSALIVE to ensure a strong reference
        // to the object.  Using this flag will cause the object to remain
        // registered until it is explicitly revoked with the Revoke() method.
        //
        // Not using this flag means that if GraphEdit remotely connects
        // to this graph and then GraphEdit exits, this object registration 
        // will be deleted, causing future attempts by GraphEdit to fail until
        // this application is restarted or until the graph is registered again.
        hr = pROT->Register(ROTFLAGS_REGISTRATIONKEEPSALIVE, pUnkGraph, 
                            pMoniker, pdwRegister);
        pMoniker->Release();
    }

    pROT->Release();
    return hr;
}





HRESULT CPlayBacDlg::OnSelectFile() 
{

	TCHAR szFilename[MAX_PATH];
	m_filename.GetWindowText(szFilename, MAX_PATH);
	
	HRESULT hr;
    // First release any existing interfaces
    ResetDirectShow();
	
	
    // Load the selected media file
    hr = PrepareMedia(szFilename);

    if (FAILED(hr))
    {
        MessageBox("unprepared media",NULL,MB_OK);
        return hr;
    }

    // Cue the first video frame
    pMC->StopWhenReady();
	
	
	return hr;
}


HRESULT CPlayBacDlg::PrepareMedia(LPTSTR lpszMovie)
{
    USES_CONVERSION;
    HRESULT hr = S_OK;

    //Say(TEXT("Loading..."));

    // Allow DirectShow to create the FilterGraph for this media file
    hr = pGB->RenderFile(T2W(lpszMovie), NULL);



    // Add our filter graph to the running object table for debugging
#ifdef DEBUG
    AddGraphToRot(pGB, &m_dwRegister);
#endif

    // Have the graph signal events via window callbacks TODO
    //hr = pME->SetNotifyWindow((OAHWND)m_hWnd, WM_GRAPHNOTIFY, 0);

    // Configure the video window

    hr = pVW->put_Owner((OAHWND) m_Screen.GetSafeHwnd());
    hr = pVW->put_WindowStyle(WS_CHILD);

    // We'll manually set the video to be visible
    hr = pVW->put_Visible(OAFALSE);

    // Place video window within the bounding rectangle
    CenterVideo();

    // Make the video window visible within the screen window.
    // If this is an audio-only file, then there won't be a video interface.
    hr = pVW->put_Visible(OATRUE);
    hr = pVW->SetWindowForeground(-1);


    //Say(TEXT("Ready"));
    return hr;
}


void CPlayBacDlg::CenterVideo(void)
{
    
	LONG width, height;
    HRESULT hr;

    // Read coordinates of video container window
    
    width =  rc.right - rc.left;
    height = rc.bottom - rc.top;

    // Ignore the video's original size and stretch to fit bounding rectangle
    hr = pVW->SetWindowPosition(rc.left, rc.top, width, height);

	/* use me later
	LONG width, height;

    // Ignore the video's original size and stretch to fit bounding rectangle
    pVW->SetWindowPosition(125, 112, nativewidth, nativeheight);
	*/

}



void CPlayBacDlg::PauseClip(void)
{
	
	pMC->Pause();
	g_psCurrent = Paused;
}



void CPlayBacDlg::PlayClip(void)
{
	pMC->Run();
	g_psCurrent = Running;
}


////////////////////////////////////////////////////////
// Buttons
void CPlayBacDlg::OnBnClickedPlay()
{		
	if(g_psCurrent == Init || g_psCurrent == Stopped)
	{
	PlayClip();
	}
	else if(g_psCurrent == Running)
	{
	PauseClip();
	}	
	else if(g_psCurrent == Paused)
	{
	PlayClip();
	}
	else if((g_psCurrent == Nothing))
	{
		MessageBox("no file loaded");
	}
}





void CPlayBacDlg::OnFileOpen138()
{
	if(pMC)
	{
		pMC->Stop();
		g_psCurrent=Init;
	}
	//used to set the rectangle for later use in CenterVideo()
	m_Screen.GetClientRect(&rc);

	SetDefaults();
    
	CFileDialog dlgFile(TRUE);
    CString title;
    CString strFilter, strDefault, strFilename;

    VERIFY(title.LoadString(AFX_IDS_OPENFILE));

    // Initialize the file extensions and descriptions
    strFilter += "Media Files (*.avi, *.mpg, *.wav, *.mid)\0";
    strFilter += (TCHAR)'\0';
    strFilter += _T("*.avi;*.mpg;*.wav;*.mid\0");
    strFilter += (TCHAR)'\0';
    dlgFile.m_ofn.nMaxCustFilter++;

    CString allFilter;
    VERIFY(allFilter.LoadString(AFX_IDS_ALLFILTER));

    // Append the "*.*" all files filter
    strFilter += allFilter;
    strFilter += (TCHAR)'\0';     // next string
    strFilter += _T("*.*\0");
    strFilter += (TCHAR)'\0\0';   // last string
    dlgFile.m_ofn.nMaxCustFilter++;

    dlgFile.m_ofn.lpstrFilter = strFilter;
    dlgFile.m_ofn.lpstrTitle  = title;
    dlgFile.m_ofn.lpstrFile   = strFilename.GetBuffer(_MAX_PATH);

    // Display the file open dialog
    INT_PTR nResult = dlgFile.DoModal();

    // If a file was selected, update the main dialog
    if (nResult == IDOK)
    {
		m_filename.SetWindowText(strFilename);
		
		// Render this file and show the first video frame, if present
        OnSelectFile();
		
    }

    strFilename.ReleaseBuffer();

	g_psCurrent=Init;

	//TODO: fix when there is time
	SetDefaults();
	OnSelectFile();
	PauseClip();
	pMS->SetTimeFormat(&TIME_FORMAT_FRAME);
	pMS->GetDuration(&duration);
	SetDlgItemInt(IDC_DURATION,duration,TRUE);

	SetDlgItemInt(IDC_ACTUALX,rc.right,TRUE);
	SetDlgItemInt(IDC_ACTUALY,rc.bottom,TRUE);

	pBV->GetVideoSize(&nativewidth, &nativeheight);
	SetDlgItemInt(IDC_NATIVEX,nativewidth,TRUE);
	SetDlgItemInt(IDC_NATIVEY,nativeheight,TRUE);
}

void CPlayBacDlg::OnControlsStep()
{
	REFERENCE_TIME currentpos1;
	pMS->GetCurrentPosition(&currentpos1);
	int currentpos2=currentpos1+1;
	currentpos1=currentpos2;

	pMS->SetPositions(&currentpos1,AM_SEEKING_AbsolutePositioning, NULL, AM_SEEKING_NoPositioning);
}



void CPlayBacDlg::OnTimer(UINT nIDEvent)
{

	RECT rc,rcmax;
	m_Screen.GetWindowRect(&rc);
	POINT framept;
	GetCursorPos(&framept);

	

	m_Screen.GetClientRect(&rcmax);

	xcurrent=framept.x-rc.left;
	ycurrent=framept.y-rc.top;
	if((xcurrent >= 0) && (ycurrent >= 0) && (xcurrent <= (rcmax.right)) && (ycurrent <= (rcmax.bottom)) )
	{
		SetDlgItemInt(IDC_EDIT1,xcurrent);
		SetDlgItemInt(IDC_EDIT2,ycurrent);
	}
	else
	{
		SetDlgItemInt(IDC_EDIT1,-1);
		SetDlgItemInt(IDC_EDIT2,-1);
	}


	if(g_psCurrent==Running || g_psCurrent==Paused)
	{
	REFERENCE_TIME workingposition;
	pMS->GetCurrentPosition(&workingposition);
	
	if(workingposition!=currentposition)
		{
		currentdatapoint=0;
		}
	}
		
	if(currentdatapoint>=25)
		currentdatapoint=24;

	SetDlgItemInt(IDC_CURRENTDATAPOINT,(currentdatapoint+1),TRUE);

	if(g_psCurrent==Running || g_psCurrent==Paused)
	{
	pMS->GetCurrentPosition(&currentposition);
	SetDlgItemInt(IDC_EDIT3,currentposition,TRUE);
	}
	
	if(currentposition<10000)
	{
		if(frameinfoarray[25*currentposition].frame!=-1)
		{
		SetDlgItemInt(IDC_EDITX1,frameinfoarray[25*currentposition].datax,TRUE);
		SetDlgItemInt(IDC_EDITY1,frameinfoarray[25*currentposition].datay,TRUE);
		}
		else
		{
		SetDlgItemInt(IDC_EDITX1,NULL,TRUE);
		SetDlgItemInt(IDC_EDITY1,NULL,TRUE);
		}
		if(frameinfoarray[25*currentposition+1].frame!=-1)
		{
		SetDlgItemInt(IDC_EDITX2,frameinfoarray[25*currentposition+1].datax,TRUE);
		SetDlgItemInt(IDC_EDITY2,frameinfoarray[25*currentposition+1].datay,TRUE);
		}
		else
		{
		SetDlgItemInt(IDC_EDITX2,NULL,TRUE);
		SetDlgItemInt(IDC_EDITY2,NULL,TRUE);
		}
		if(frameinfoarray[25*currentposition+2].frame!=-1)
		{
		SetDlgItemInt(IDC_EDITX3,frameinfoarray[25*currentposition+2].datax,TRUE);
		SetDlgItemInt(IDC_EDITY3,frameinfoarray[25*currentposition+2].datay,TRUE);
		}
		else
		{
		SetDlgItemInt(IDC_EDITX3,NULL,TRUE);
		SetDlgItemInt(IDC_EDITY3,NULL,TRUE);
		}
		if(frameinfoarray[25*currentposition+3].frame!=-1)
		{
		SetDlgItemInt(IDC_EDITX4,frameinfoarray[25*currentposition+3].datax,TRUE);
		SetDlgItemInt(IDC_EDITY4,frameinfoarray[25*currentposition+3].datay,TRUE);
		}
		else
		{
		SetDlgItemInt(IDC_EDITX4,NULL,TRUE);
		SetDlgItemInt(IDC_EDITY4,NULL,TRUE);
		}
		if(frameinfoarray[25*currentposition+4].frame!=-1)
		{
		SetDlgItemInt(IDC_EDITX5,frameinfoarray[25*currentposition+4].datax,TRUE);
		SetDlgItemInt(IDC_EDITY5,frameinfoarray[25*currentposition+4].datay,TRUE);
		}
		else
		{
		SetDlgItemInt(IDC_EDITX5,NULL,TRUE);
		SetDlgItemInt(IDC_EDITY5,NULL,TRUE);
		}
		if(frameinfoarray[25*currentposition+5].frame!=-1)
		{
		SetDlgItemInt(IDC_EDITX6,frameinfoarray[25*currentposition+5].datax,TRUE);
		SetDlgItemInt(IDC_EDITY6,frameinfoarray[25*currentposition+5].datay,TRUE);
		}
		else
		{
		SetDlgItemInt(IDC_EDITX6,NULL,TRUE);
		SetDlgItemInt(IDC_EDITY6,NULL,TRUE);
		}
		if(frameinfoarray[25*currentposition+6].frame!=-1)
		{
		SetDlgItemInt(IDC_EDITX7,frameinfoarray[25*currentposition+6].datax,TRUE);
		SetDlgItemInt(IDC_EDITY7,frameinfoarray[25*currentposition+6].datay,TRUE);
		}
		else
		{
		SetDlgItemInt(IDC_EDITX7,NULL,TRUE);
		SetDlgItemInt(IDC_EDITY7,NULL,TRUE);
		}
		if(frameinfoarray[25*currentposition+7].frame!=-1)
		{
		SetDlgItemInt(IDC_EDITX8,frameinfoarray[25*currentposition+7].datax,TRUE);
		SetDlgItemInt(IDC_EDITY8,frameinfoarray[25*currentposition+7].datay,TRUE);
		}
		else
		{
		SetDlgItemInt(IDC_EDITX8,NULL,TRUE);
		SetDlgItemInt(IDC_EDITY8,NULL,TRUE);
		}
		if(frameinfoarray[25*currentposition+8].frame!=-1)
		{
		SetDlgItemInt(IDC_EDITX9,frameinfoarray[25*currentposition+8].datax,TRUE);
		SetDlgItemInt(IDC_EDITY9,frameinfoarray[25*currentposition+8].datay,TRUE);
		}
		else
		{
		SetDlgItemInt(IDC_EDITX9,NULL,TRUE);
		SetDlgItemInt(IDC_EDITY9,NULL,TRUE);
		}
		if(frameinfoarray[25*currentposition+9].frame!=-1)
		{
		SetDlgItemInt(IDC_EDITX10,frameinfoarray[25*currentposition+9].datax,TRUE);
		SetDlgItemInt(IDC_EDITY10,frameinfoarray[25*currentposition+9].datay,TRUE);
		}
		else
		{
		SetDlgItemInt(IDC_EDITX10,NULL,TRUE);
		SetDlgItemInt(IDC_EDITY10,NULL,TRUE);
		}
		if(frameinfoarray[25*currentposition+10].frame!=-1)
		{
		SetDlgItemInt(IDC_EDITX11,frameinfoarray[25*currentposition+10].datax,TRUE);
		SetDlgItemInt(IDC_EDITY11,frameinfoarray[25*currentposition+10].datay,TRUE);
		}
		else
		{
		SetDlgItemInt(IDC_EDITX11,NULL,TRUE);
		SetDlgItemInt(IDC_EDITY11,NULL,TRUE);
		}
		if(frameinfoarray[25*currentposition+11].frame!=-1)
		{
		SetDlgItemInt(IDC_EDITX12,frameinfoarray[25*currentposition+11].datax,TRUE);
		SetDlgItemInt(IDC_EDITY12,frameinfoarray[25*currentposition+11].datay,TRUE);
		}
		else
		{
		SetDlgItemInt(IDC_EDITX12,NULL,TRUE);
		SetDlgItemInt(IDC_EDITY12,NULL,TRUE);
		}
		if(frameinfoarray[25*currentposition+12].frame!=-1)
		{
		SetDlgItemInt(IDC_EDITX13,frameinfoarray[25*currentposition+12].datax,TRUE);
		SetDlgItemInt(IDC_EDITY13,frameinfoarray[25*currentposition+12].datay,TRUE);
		}
		else
		{
		SetDlgItemInt(IDC_EDITX13,NULL,TRUE);
		SetDlgItemInt(IDC_EDITY13,NULL,TRUE);
		}
		if(frameinfoarray[25*currentposition+13].frame!=-1)
		{
		SetDlgItemInt(IDC_EDITX14,frameinfoarray[25*currentposition+13].datax,TRUE);
		SetDlgItemInt(IDC_EDITY14,frameinfoarray[25*currentposition+13].datay,TRUE);
		}
		else
		{
		SetDlgItemInt(IDC_EDITX14,NULL,TRUE);
		SetDlgItemInt(IDC_EDITY14,NULL,TRUE);
		}
		if(frameinfoarray[25*currentposition+14].frame!=-1)
		{
		SetDlgItemInt(IDC_EDITX15,frameinfoarray[25*currentposition+14].datax,TRUE);
		SetDlgItemInt(IDC_EDITY15,frameinfoarray[25*currentposition+14].datay,TRUE);
		}
		else
		{
		SetDlgItemInt(IDC_EDITX15,NULL,TRUE);
		SetDlgItemInt(IDC_EDITY15,NULL,TRUE);
		}
		if(frameinfoarray[25*currentposition+15].frame!=-1)
		{
		SetDlgItemInt(IDC_EDITX16,frameinfoarray[25*currentposition+15].datax,TRUE);
		SetDlgItemInt(IDC_EDITY16,frameinfoarray[25*currentposition+15].datay,TRUE);
		}
		else
		{
		SetDlgItemInt(IDC_EDITX16,NULL,TRUE);
		SetDlgItemInt(IDC_EDITY16,NULL,TRUE);
		}
		if(frameinfoarray[25*currentposition+16].frame!=-1)
		{
		SetDlgItemInt(IDC_EDITX17,frameinfoarray[25*currentposition+16].datax,TRUE);
		SetDlgItemInt(IDC_EDITY17,frameinfoarray[25*currentposition+16].datay,TRUE);
		}
		else
		{
		SetDlgItemInt(IDC_EDITX17,NULL,TRUE);
		SetDlgItemInt(IDC_EDITY17,NULL,TRUE);
		}
		if(frameinfoarray[25*currentposition+17].frame!=-1)
		{
		SetDlgItemInt(IDC_EDITX18,frameinfoarray[25*currentposition+17].datax,TRUE);
		SetDlgItemInt(IDC_EDITY18,frameinfoarray[25*currentposition+17].datay,TRUE);
		}
		else
		{
		SetDlgItemInt(IDC_EDITX18,NULL,TRUE);
		SetDlgItemInt(IDC_EDITY18,NULL,TRUE);
		}
		if(frameinfoarray[25*currentposition+18].frame!=-1)
		{
		SetDlgItemInt(IDC_EDITX19,frameinfoarray[25*currentposition+18].datax,TRUE);
		SetDlgItemInt(IDC_EDITY19,frameinfoarray[25*currentposition+18].datay,TRUE);
		}
		else
		{
		SetDlgItemInt(IDC_EDITX19,NULL,TRUE);
		SetDlgItemInt(IDC_EDITY19,NULL,TRUE);
		}
		if(frameinfoarray[25*currentposition+19].frame!=-1)
		{
		SetDlgItemInt(IDC_EDITX20,frameinfoarray[25*currentposition+19].datax,TRUE);
		SetDlgItemInt(IDC_EDITY20,frameinfoarray[25*currentposition+19].datay,TRUE);
		}
		else
		{
		SetDlgItemInt(IDC_EDITX20,NULL,TRUE);
		SetDlgItemInt(IDC_EDITY20,NULL,TRUE);
		}
		if(frameinfoarray[25*currentposition+20].frame!=-1)
		{
		SetDlgItemInt(IDC_EDITX21,frameinfoarray[25*currentposition+20].datax,TRUE);
		SetDlgItemInt(IDC_EDITY21,frameinfoarray[25*currentposition+20].datay,TRUE);
		}
		else
		{
		SetDlgItemInt(IDC_EDITX21,NULL,TRUE);
		SetDlgItemInt(IDC_EDITY21,NULL,TRUE);
		}
		if(frameinfoarray[25*currentposition+21].frame!=-1)
		{
		SetDlgItemInt(IDC_EDITX22,frameinfoarray[25*currentposition+21].datax,TRUE);
		SetDlgItemInt(IDC_EDITY22,frameinfoarray[25*currentposition+21].datay,TRUE);
		}
		else
		{
		SetDlgItemInt(IDC_EDITX22,NULL,TRUE);
		SetDlgItemInt(IDC_EDITY22,NULL,TRUE);
		}
		if(frameinfoarray[25*currentposition+22].frame!=-1)
		{
		SetDlgItemInt(IDC_EDITX23,frameinfoarray[25*currentposition+22].datax,TRUE);
		SetDlgItemInt(IDC_EDITY23,frameinfoarray[25*currentposition+22].datay,TRUE);
		}
		else
		{
		SetDlgItemInt(IDC_EDITX23,NULL,TRUE);
		SetDlgItemInt(IDC_EDITY23,NULL,TRUE);
		}
		if(frameinfoarray[25*currentposition+23].frame!=-1)
		{
		SetDlgItemInt(IDC_EDITX24,frameinfoarray[25*currentposition+23].datax,TRUE);
		SetDlgItemInt(IDC_EDITY24,frameinfoarray[25*currentposition+23].datay,TRUE);
		}
		else
		{
		SetDlgItemInt(IDC_EDITX24,NULL,TRUE);
		SetDlgItemInt(IDC_EDITY24,NULL,TRUE);
		}
		if(frameinfoarray[25*currentposition+24].frame!=-1)
		{
		SetDlgItemInt(IDC_EDITX25,frameinfoarray[25*currentposition+24].datax,TRUE);
		SetDlgItemInt(IDC_EDITY25,frameinfoarray[25*currentposition+24].datay,TRUE);
		}
		else
		{
		SetDlgItemInt(IDC_EDITX25,NULL,TRUE);
		SetDlgItemInt(IDC_EDITY25,NULL,TRUE);
		}
	}
	else
	{
		if(sizewarning!=1)
		{
		sizewarning=1;
		MessageBox("This player can not take data after 10000 frames.\nPlease crop the video size",NULL,MB_OK);
		}
	}

	CDialog::OnTimer(nIDEvent);
}


BOOL CPlayBacDlg::PreTranslateMessage(MSG* pMsg)
{
	if(m_hAccel)
	{
    if(::TranslateAccelerator(m_hWnd, m_hAccel, pMsg))
		return(TRUE);
	}


	return CDialog::PreTranslateMessage(pMsg);
}

void CPlayBacDlg::OnControlsTakedatapoint()
{
	if(currentposition<=10000)
	{
		RECT rc, rcmax;
		m_Screen.GetWindowRect(&rc);
		POINT framept;
		GetCursorPos(&framept);

		m_Screen.GetClientRect(&rcmax);
		
		if((xcurrent >= 0) && (ycurrent >= 0) && (xcurrent <= (rcmax.right)) && (ycurrent <= (rcmax.bottom)) )
		{
			frameinfoarray[25*currentposition+currentdatapoint].frame=currentposition;
			frameinfoarray[25*currentposition+currentdatapoint].datax=xcurrent;
			frameinfoarray[25*currentposition+currentdatapoint].datay=ycurrent;
			currentdatapoint++;
		}
		else
		{
			if(sizewarning!=0)
			{
			MessageBox("The data point must be within the window",NULL,MB_OK);
			sizewarning=1;
			}
		}
	}
	else
	{
		MessageBox("You can not take data points after the 10000th frame.",NULL,MB_OK);
	}
}



void CPlayBacDlg::OnFileExportdata()
{
	CFile file("data.dat",CFile::modeCreate|CFile::modeWrite);
	CString hello="hello";
	file.Write(hello, hello.GetLength());
	file.Close;
}

void CPlayBacDlg::OnBnClickedBackward()
{
	REFERENCE_TIME currentpos1;
	pMS->GetCurrentPosition(&currentpos1);
	int currentpos2=currentpos1-1;
	currentpos1=currentpos2;

	pMS->SetPositions(&currentpos1,AM_SEEKING_AbsolutePositioning, NULL, AM_SEEKING_NoPositioning);
}

void CPlayBacDlg::OnBnClickedForward()
{
	REFERENCE_TIME currentpos1;
	pMS->GetCurrentPosition(&currentpos1);
	int currentpos2=currentpos1+1;
	currentpos1=currentpos2;

	pMS->SetPositions(&currentpos1,AM_SEEKING_AbsolutePositioning, NULL, AM_SEEKING_NoPositioning);
}

void CPlayBacDlg::OnBnClickedBackward10()
{
	REFERENCE_TIME currentpos1;
	pMS->GetCurrentPosition(&currentpos1);
	int currentpos2=currentpos1-10;
	currentpos1=currentpos2;

	pMS->SetPositions(&currentpos1,AM_SEEKING_AbsolutePositioning, NULL, AM_SEEKING_NoPositioning);
}

void CPlayBacDlg::OnBnClickedForward10()
{
	REFERENCE_TIME currentpos1;
	pMS->GetCurrentPosition(&currentpos1);
	int currentpos2=currentpos1+10;
	currentpos1=currentpos2;

	pMS->SetPositions(&currentpos1,AM_SEEKING_AbsolutePositioning, NULL, AM_SEEKING_NoPositioning);
}
void CPlayBacDlg::OnBnClickedF100()
{
	REFERENCE_TIME currentpos1;
	pMS->GetCurrentPosition(&currentpos1);
	int currentpos2=currentpos1+100;
	currentpos1=currentpos2;

	pMS->SetPositions(&currentpos1,AM_SEEKING_AbsolutePositioning, NULL, AM_SEEKING_NoPositioning);
}

void CPlayBacDlg::OnBnClickedF1000()
{
	REFERENCE_TIME currentpos1;
	pMS->GetCurrentPosition(&currentpos1);
	int currentpos2=currentpos1+1000;
	currentpos1=currentpos2;

	pMS->SetPositions(&currentpos1,AM_SEEKING_AbsolutePositioning, NULL, AM_SEEKING_NoPositioning);
}

void CPlayBacDlg::OnBnClickedB100()
{
	REFERENCE_TIME currentpos1;
	pMS->GetCurrentPosition(&currentpos1);
	if(currentpos1>=100)
	{
	int currentpos2=currentpos1-100;
	currentpos1=currentpos2;
	pMS->SetPositions(&currentpos1,AM_SEEKING_AbsolutePositioning, NULL, AM_SEEKING_NoPositioning);
	}
	else
	{
	int currentpos2=0;
	currentpos1=currentpos2;
	pMS->SetPositions(&currentpos1,AM_SEEKING_AbsolutePositioning, NULL, AM_SEEKING_NoPositioning);
	}


}

void CPlayBacDlg::OnBnClickedB1000()
{
	REFERENCE_TIME currentpos1;
	pMS->GetCurrentPosition(&currentpos1);
	if(currentpos1>=1000)
	{
	int currentpos2=currentpos1-1000;
	currentpos1=currentpos2;
	pMS->SetPositions(&currentpos1,AM_SEEKING_AbsolutePositioning, NULL, AM_SEEKING_NoPositioning);
	}
	else
	{
	int currentpos2=0;
	currentpos1=currentpos2;
	pMS->SetPositions(&currentpos1,AM_SEEKING_AbsolutePositioning, NULL, AM_SEEKING_NoPositioning);
	}
}


CString CPlayBacDlg::InttoString(int value)
{
	//taken from http://www.planet-source-code.com/vb/scripts/ShowCode.asp?lngWId=3&txtCodeId=8216
	CString number;
    int temp;
    bool neg= false;
    //Proof if the number has a negiote sign
    //     

        if ((value * (-1)) > value){
        value *=-1;
        neg=true;
    }
		int intlength;
		
		if(value > 999)
			intlength=4;
		else if(value > 99)
			intlength=3;
		else if(value >9)
			intlength=2;
		else
			intlength=1;

        for (int z = intlength ;z>=1;z--){
        //Modulo to get the last number 
        temp = value % 10;
        //add Number to the String
        number = ((char)(temp + 48))+ number;
        //Substract the last number from the Ing
        //     eter
        value = (value - temp) /10;
        temp *= 10 ;
    }

    //Set the sign again
    if (neg==true){ number="-" + number; }
    return number;
}

void CPlayBacDlg::OnBnClickedPost()
{
	CString postdata=NULL;
	m_filename.GetWindowText(postdata);
	postdata+="\n\twidth\theight\n";
	postdata+="size\t"+InttoString(rc.right)+"\t" + InttoString(rc.bottom)+"\n";
	postdata=postdata+"nativesize\t"+InttoString(nativewidth) +"\t" +InttoString(nativeheight)+"\n\n";
	for(int count1 =0; count1<1000; count1++)
	{
		if(frameinfoarray[25*count1].frame!=-1)
		{
		//frame marker
		postdata=postdata + InttoString(count1) +"\t";
		//x values
		for(int count2=0; count2<25;count2++)
		{
		if(frameinfoarray[25*count1+count2].frame!=-1)
		{		
			postdata = postdata+InttoString(frameinfoarray[25*count1+count2].datax);
			postdata = postdata + "\t";
		}
		}
		postdata=postdata + "\n\t";
		//y values
		for(int count2=0; count2<25;count2++)
		{
		if(frameinfoarray[25*count1+count2].frame!=-1)
		{
			postdata = postdata + InttoString(frameinfoarray[25*count1+count2].datay);
			postdata = postdata + "\t";
		}
		}
		postdata=postdata + "\n";
		}
	}

	SetDlgItemText(IDC_EDITPOSTDATA,postdata);
}


void CPlayBacDlg::OnControlsStepback148()
{
	REFERENCE_TIME currentpos1;
	pMS->GetCurrentPosition(&currentpos1);
	int currentpos2=currentpos1-1;
	currentpos1=currentpos2;

	pMS->SetPositions(&currentpos1,AM_SEEKING_AbsolutePositioning, NULL, AM_SEEKING_NoPositioning);
}

void CPlayBacDlg::OnBnClickedResize()
{
	int xresize=0; 
	int	yresize=0;
	xresize=GetDlgItemInt(IDC_XRESIZE,NULL,TRUE);
	yresize=GetDlgItemInt(IDC_YRESIZE,NULL,TRUE);

	if( g_psCurrent!=Nothing && (xresize <= 800) && (yresize <= 600) && xresize>0 && yresize >0)
	{
	m_Screen.SetWindowPos(NULL,rc.left,rc.top,xresize,yresize,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
	pVW->SetWindowPosition(rc.left, rc.top, (xresize), (yresize));
	m_Screen.GetClientRect(&rc);
	SetDlgItemInt(IDC_ACTUALX,rc.right,TRUE);
	SetDlgItemInt(IDC_ACTUALY,rc.bottom,TRUE);
	}
	else
	{
		MessageBox("The desired area is not possible.",NULL,MB_OK);
	}

}


void CPlayBacDlg::OnBnClickedButton3()
{
	int delpoint;
	delpoint=GetDlgItemInt(IDC_DELPOINT,NULL,TRUE);
	if(frameinfoarray[25*currentposition+delpoint-1].frame!=-1)
	{
		if(delpoint>0 && delpoint<26)
		{
			frameinfoarray[25*currentposition+delpoint-1].frame=-1;
			frameinfoarray[25*currentposition+delpoint-1].datax=-1;
			frameinfoarray[25*currentposition+delpoint-1].datay=-1;
			currentdatapoint=delpoint-1;
		}
		else
		{
			MessageBox("You must enter a point between 1 and 25.",NULL,MB_OK);
		}
	}
	else
	{
		MessageBox("There is no data point there.",NULL, MB_OK);
	}
}

void CPlayBacDlg::OnBnClickedSetdatapointbutton()
{
	int setdatapoint;
	setdatapoint=GetDlgItemInt(IDC_SETDATAPOINT,NULL,TRUE);
	if(setdatapoint<26 && setdatapoint>0)
	{
	currentdatapoint=setdatapoint-1;
	}
	else
	{
	MessageBox("The data point must be between 1 and 25",NULL,MB_OK);
	}
}

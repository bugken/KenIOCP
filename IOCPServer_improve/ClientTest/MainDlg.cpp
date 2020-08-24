// MainDlg.cpp : ʵ���ļ�
#include "stdafx.h"
#include "ClientTest.h"
#include "MainDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// ����Ӧ�ó��򡰹��ڡ��˵���� CAboutDlg �Ի���
class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

	// �Ի�������
	enum { IDD = IDD_ABOUTBOX };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);

protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()

// CMainDlg �Ի���
CMainDlg::CMainDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CMainDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CMainDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CMainDlg, CDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDOK, &CMainDlg::OnBnClickedOk)
	ON_BN_CLICKED(IDC_STOP, &CMainDlg::OnBnClickedStop)
	ON_BN_CLICKED(IDCANCEL, &CMainDlg::OnBnClickedCancel)
	ON_MESSAGE(WM_ADD_LIST_ITEM, OnAddListItem)
	ON_WM_DESTROY()
END_MESSAGE_MAP()

// CMainDlg ��Ϣ�������
BOOL CMainDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
	// ��������...���˵�����ӵ�ϵͳ�˵��С�
	// IDM_ABOUTBOX ������ϵͳ���Χ�ڡ�
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);
	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		CString strAboutMenu;
		strAboutMenu.LoadString(IDS_ABOUTBOX);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}
	// ���ô˶Ի����ͼ�ꡣ��Ӧ�ó��������ڲ��ǶԻ���ʱ����ܽ��Զ�
	//  ִ�д˲���
	SetIcon(m_hIcon, TRUE);			// ���ô�ͼ��
	SetIcon(m_hIcon, FALSE);		// ����Сͼ��
	// ��ʼ��������Ϣ
	this->InitGUI();
	return TRUE;  // ���ǽ��������õ��ؼ������򷵻� TRUE
}

void CMainDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

// �����Ի��������С����ť������Ҫ����Ĵ���
//  �����Ƹ�ͼ�ꡣ����ʹ���ĵ�/��ͼģ�͵� MFC Ӧ�ó���
//  �⽫�ɿ���Զ���ɡ�
void CMainDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // ���ڻ��Ƶ��豸������
		SendMessage(WM_ICONERASEBKGND,
			reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);
		// ʹͼ���ڹ����������о���
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;
		// ����ͼ��
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

//���û��϶���С������ʱϵͳ���ô˺���ȡ�ù��
//��ʾ��
HCURSOR CMainDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

//////////////////////////////////////////////////////////////////////
// ��ʼ��������Ϣ
HWND g_hWnd = 0;
void CMainDlg::InitGUI()
{
	// ��ʼ��Socket��
	if (false == m_Client.LoadSocketLib())
	{
		AfxMessageBox(_T("����Winsock 2.2ʧ�ܣ����������޷����У�"));
		PostQuitMessage(0);
	}
	// ���ñ���IP��ַ
	SetDlgItemText(IDC_IPADDRESS_SERVER, m_Client.GetLocalIP());
	// ����Ĭ�϶˿�
	SetDlgItemInt(IDC_EDIT_PORT, DEFAULT_PORT);
	// ����Ĭ�ϵĲ����̷߳��ʹ���
	SetDlgItemInt(IDC_EDIT_TIMES, DEFAULT_TIMES);
	// ����Ĭ�ϵĲ����߳���
	SetDlgItemInt(IDC_EDIT_THREADS, DEFAULT_THREADS);
	// ����Ĭ�Ϸ�����Ϣ
	SetDlgItemText(IDC_EDIT_MESSAGE, DEFAULT_MESSAGE);
	// ��ʼ���б�
	this->InitListCtrl();
	// ��־��Ϣ�Ĵ���
	g_hWnd = this->m_hWnd;
	//LPVOID pfn = (LPVOID)AddInformation;
	//m_Client.SetLogFunc((LOG_FUNC)pfn);
	// ����������ָ��
	m_Client.SetMainDlg(this);
}

void CMainDlg::AddInformation(const string& strInfo)
{
	int len = strInfo.size();
	if (g_hWnd && len > 0)
	{
		char* pStr = new char[len +1];
		if (pStr)
		{
			memset(pStr, 0, len + 1);
			strncpy(pStr, strInfo.c_str(), len);		
			if (!::PostMessage(g_hWnd, WM_ADD_LIST_ITEM, 
				0, (LPARAM)pStr))
			{
				delete[]pStr;
			}
		}
	}
}

LRESULT CMainDlg::OnAddListItem(WPARAM wParam, LPARAM lParam)
{
	if (lParam)
	{
		char* pStr = ((char*)lParam);
		CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST_INFO);
		int count = pList->GetItemCount();
		while (count > MAX_LIST_ITEM_COUNT)
		{//�б������̫�࣬�϶���������
			pList->DeleteItem(--count);
		}
		pList->InsertItem(0, pStr);
		delete []pStr;
	}
	return 0;
}

///////////////////////////////////////////////////////////////////////
//	��ʼ��List Control
void CMainDlg::InitListCtrl()
{
	CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST_INFO);
	pList->SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	pList->InsertColumn(0, "INFORMATION", LVCFMT_LEFT, 500);
}

///////////////////////////////////////////////////////////////////////
// ��ʼ����
void CMainDlg::OnBnClickedOk()
{
	int nPort = GetDlgItemInt(IDC_EDIT_PORT);
	int nTimes = GetDlgItemInt(IDC_EDIT_TIMES);
	int nThreads = GetDlgItemInt(IDC_EDIT_THREADS);
	CString strIP, strMessage;
	GetDlgItemText(IDC_IPADDRESS_SERVER, strIP);
	GetDlgItemText(IDC_EDIT_MESSAGE, strMessage);
	if (strIP == _T("") || strMessage == _T("")
		|| nPort <= 0 || nThreads <= 0)
	{
		AfxMessageBox(_T("������Ϸ��Ĳ�����"));
		return;
	}
	CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST_INFO);
	pList->DeleteAllItems();

	// ��CClient���ò���
	m_Client.SetIP(strIP);
	m_Client.SetPort(nPort);
	m_Client.SetTimes(nTimes);
	m_Client.SetThreads(nThreads);
	m_Client.SetMessage(strMessage);
	// ��ʼ
	if (!m_Client.Start())
	{
		AfxMessageBox(_T("����ʧ�ܣ�"));
		return;
	}
	m_Client.ShowMessage("���Կ�ʼ");
	GetDlgItem(IDOK)->EnableWindow(FALSE);
	GetDlgItem(IDC_STOP)->EnableWindow(TRUE);
}


//////////////////////////////////////////////////////////////////////
//	ֹͣ����
void CMainDlg::OnBnClickedStop()
{
	// ֹͣ
	m_Client.Stop();
	GetDlgItem(IDC_STOP)->EnableWindow(FALSE);
	GetDlgItem(IDOK)->EnableWindow(TRUE);
	m_Client.ShowMessage("����ֹͣ");
}

/////////////////////////////////////////////////////////////////////
//	�˳�
void CMainDlg::OnBnClickedCancel()
{
	// ֹͣ����
	m_Client.Stop();
	// ж��Socket��
	m_Client.UnloadSocketLib();
	OnCancel();
}

//////////////////////////////////////////////////////////////////////
//	�Ի�������ʱ�������ͷ���Դ
void CMainDlg::OnDestroy()
{
	OnBnClickedCancel();
	CDialog::OnDestroy();
}

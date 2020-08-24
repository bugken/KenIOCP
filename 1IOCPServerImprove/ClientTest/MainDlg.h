// MainDlg.h : ͷ�ļ�
#pragma once
#include "Client.h"

#define MAX_LIST_ITEM_COUNT 1000
#define WM_ADD_LIST_ITEM (WM_USER + 100)  

// CMainDlg �Ի���
class CMainDlg : public CDialog
{
public:
	CMainDlg(CWnd* pParent = NULL);	// ��׼���캯��

	// �Ի�������
	enum { IDD = IDD_CLIENT_DIALOG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);

protected:
	HICON m_hIcon;
	// ���ɵ���Ϣӳ�亯��
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	// ��ʼ����
	afx_msg void OnBnClickedOk();
	// ֹͣ����
	afx_msg void OnBnClickedStop();
	// �˳�
	afx_msg void OnBnClickedCancel();
	// �Ի�������
	afx_msg void OnDestroy();
	// �б�����ݵ�ˢ�£�����б��
	afx_msg LRESULT OnAddListItem(WPARAM wParam, LPARAM lParam);
	DECLARE_MESSAGE_MAP()

private:
	// ��ʼ��������Ϣ
	void InitGUI();
	// ��ʼ��List�ؼ�
	void InitListCtrl();
public:
	// Ϊ�����������Ϣ��Ϣ(����CIocpModel�е���)
	static void AddInformation(const string& strInfo);

private:
	CClient m_Client; // �ͻ������Ӷ���
};

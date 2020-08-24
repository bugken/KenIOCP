/**********************************************************
ԭ�����ߣ�http://blog.csdn.net/piggyxp/article/details/6922277
�޸�ʱ�䣺2013��1��11��22:45:10
**********************************************************/

#pragma once

#include <winsock2.h>
#include <MSWSock.h>
#include <vector>
#include <list>
#include <algorithm>
#include <string>
#include "ClientParse.h"

#pragma comment(lib,"ws2_32.lib")

//���������� (1024*8)
#define MAX_BUFFER_LEN        8192  
//Ĭ�϶˿�
#define DEFAULT_PORT          12345    
//Ĭ��IP��ַ
#define DEFAULT_IP            "127.0.0.1"


/****************************************************************
BOOL WINAPI GetQueuedCompletionStatus(
__in   HANDLE CompletionPort,
__out  LPDWORD lpNumberOfBytes,
__out  PULONG_PTR lpCompletionKey,
__out  LPOVERLAPPED *lpOverlapped,
__in   DWORD dwMilliseconds
);
lpCompletionKey [out] ��Ӧ��PER_SOCKET_CONTEXT�ṹ������CreateIoCompletionPort���׽��ֵ���ɶ˿�ʱ���룻
A pointer to a variable that receives the completion key value associated with the file handle whose I/O operation has completed.
A completion key is a per-file key that is specified in a call to CreateIoCompletionPort.

lpOverlapped [out] ��Ӧ��PER_IO_CONTEXT�ṹ���磺����accept����ʱ������AcceptEx����ʱ���룻
A pointer to a variable that receives the address of the OVERLAPPED structure that was specified when the completed I/O operation was started.

****************************************************************/

// ����ɶ˿���Ͷ�ݵ�I/O����������
typedef enum _OPERATION_TYPE
{
	ACCEPT_POSTED,                     // ��־Ͷ�ݵ�Accept����
	SEND_POSTED,                       // ��־Ͷ�ݵ��Ƿ��Ͳ���
	RECV_POSTED,                       // ��־Ͷ�ݵ��ǽ��ղ���
	NULL_POSTED                        // ���ڳ�ʼ����������

}OPERATION_TYPE;


//ÿ���׽��ֲ���(�磺AcceptEx, WSARecv, WSASend��)��Ӧ�����ݽṹ��OVERLAPPED�ṹ(��ʶ���β���)���������׽��֣����������������ͣ�
typedef struct _PER_IO_CONTEXT
{
	OVERLAPPED     m_Overlapped;                               // ÿһ���ص�����������ص��ṹ(���ÿһ��Socket��ÿһ����������Ҫ��һ��)              
	SOCKET         m_sockAccept;                               // ������������ʹ�õ�Socket
	WSABUF         m_wsaBuf;                                   // WSA���͵Ļ����������ڸ��ص�������������
	char           m_szBuffer[MAX_BUFFER_LEN];                 // �����WSABUF�������ַ��Ļ�����
	OPERATION_TYPE m_OpType;                                   // ��ʶ�������������(��Ӧ�����ö��)

	DWORD			m_nTotalBytes;	//�����ܵ��ֽ���
	DWORD			m_nSendBytes;	//�Ѿ����͵��ֽ�������δ��������������Ϊ0

	CClientParse   *m_pClientParse;	//�ͻ���ҵ����

	//���캯��
	_PER_IO_CONTEXT()
	{
		ZeroMemory(&m_Overlapped, sizeof(m_Overlapped));
		ZeroMemory(m_szBuffer, MAX_BUFFER_LEN);
		m_sockAccept = INVALID_SOCKET;
		m_wsaBuf.buf = m_szBuffer;
		m_wsaBuf.len = MAX_BUFFER_LEN;
		m_OpType = NULL_POSTED;

		m_nTotalBytes = 0;
		m_nSendBytes = 0;
		m_pClientParse = new CClientParse();
	}
	//��������
	~_PER_IO_CONTEXT()
	{
		if (m_sockAccept != INVALID_SOCKET)
		{
			closesocket(m_sockAccept);
			m_sockAccept = INVALID_SOCKET;
		}
		if (m_pClientParse != NULL)
		{
			//����Լ��ĵ�¼״̬
			CClientParse::DeleteSelfLoginState(m_pClientParse);
			delete m_pClientParse;
			m_pClientParse = NULL;
		}
	}
	//���û���������
	void ResetBuffer()
	{
		ZeroMemory(m_szBuffer, MAX_BUFFER_LEN);
		m_wsaBuf.buf = m_szBuffer;
		m_wsaBuf.len = MAX_BUFFER_LEN;
	}

	//��ȡ��ǰ�û���
	std::string GetUserName()
	{
		if (m_pClientParse)
			return m_pClientParse->GetUserName();
		return "";
	}

} PER_IO_CONTEXT, *PPER_IO_CONTEXT;


//ÿ��SOCKET��Ӧ�����ݽṹ(����GetQueuedCompletionStatus����)��-
//SOCKET����SOCKET��Ӧ�Ŀͻ��˵�ַ�������ڸ�SOCKET��������(��Ӧ�ṹPER_IO_CONTEXT)��
typedef struct _PER_SOCKET_CONTEXT
{
	SOCKET      m_Socket;                                  //���ӿͻ��˵�socket
	SOCKADDR_IN m_ClientAddr;                              //�ͻ��˵�ַ
	std::list<_PER_IO_CONTEXT*> m_arrayIoContext;             //�׽��ֲ�����������WSARecv��WSASend����һ��PER_IO_CONTEXT

	//���캯��
	_PER_SOCKET_CONTEXT()
	{
		m_Socket = INVALID_SOCKET;
		memset(&m_ClientAddr, 0, sizeof(m_ClientAddr));
	}

	//��������
	~_PER_SOCKET_CONTEXT()
	{
		if (m_Socket != INVALID_SOCKET)
		{
			closesocket(m_Socket);
			m_Socket = INVALID_SOCKET;
		}
		// �ͷŵ����е�IO����������
		for (auto pIoContext : m_arrayIoContext)
		{
			delete pIoContext;
		}
		m_arrayIoContext.clear();
	}

	//�����׽��ֲ���ʱ�����ô˺�������PER_IO_CONTEX�ṹ
	_PER_IO_CONTEXT* GetNewIoContext()
	{
		_PER_IO_CONTEXT* p = new _PER_IO_CONTEXT;
		m_arrayIoContext.push_back(p);

		return p;
	}

	// ���������Ƴ�һ��ָ����IoContext
	void RemoveContext(_PER_IO_CONTEXT* pContext)
	{
		if (NULL == pContext)
			return;

		m_arrayIoContext.remove(pContext);
	}

} PER_SOCKET_CONTEXT, *PPER_SOCKET_CONTEXT;



// �������̵߳��̲߳���
class CIOCPModel;
typedef struct _tagThreadParams_WORKER
{
	CIOCPModel* pIOCPModel;                                   //��ָ�룬���ڵ������еĺ���
	int         nThreadNo;                                    //�̱߳��

} THREADPARAMS_WORKER, *PTHREADPARAM_WORKER;

// CIOCPModel��
class CIOCPModel
{
public:
	CIOCPModel(void);
	~CIOCPModel(void);

public:
	// ����Socket��
	bool LoadSocketLib();
	// ж��Socket�⣬��������
	void UnloadSocketLib() { WSACleanup(); }

	// ����������
	bool Start();
	//	ֹͣ������
	void Stop();

	// ��ñ�����IP��ַ
	std::string GetLocalIP();

	// ���ü����˿�
	void SetPort(const int& nPort) { m_nPort = nPort; }

	//Ͷ��WSASend�����ڷ�������
	bool PostWrite(PER_IO_CONTEXT* pAcceptIoContext);

	//Ͷ��WSARecv���ڽ�������
	bool PostRecv(PER_IO_CONTEXT* pIoContext);

protected:
	// ��ʼ��IOCP
	bool _InitializeIOCP();
	// ��ʼ��Socket
	bool _InitializeListenSocket();
	// ����ͷ���Դ
	void _DeInitialize();

	//Ͷ��AcceptEx����
	bool _PostAccept(PER_IO_CONTEXT* pAcceptIoContext);
	//���пͻ��������ʱ�򣬽��д���
	bool _DoAccpet(PER_SOCKET_CONTEXT* pSocketContext, PER_IO_CONTEXT* pIoContext);

	//���ӳɹ�ʱ�����ݵ�һ���Ƿ���յ����Կͻ��˵����ݽ��е���
	bool _DoFirstRecvWithData(PER_IO_CONTEXT* pIoContext);
	bool _DoFirstRecvWithoutData(PER_IO_CONTEXT* pIoContext);
	//���ݵ����������pIoContext������
	bool _DoRecv(PER_SOCKET_CONTEXT* pSocketContext, PER_IO_CONTEXT* pIoContext);

	//���ͻ���socket�������Ϣ�洢��������
	void _AddToContextList(PER_SOCKET_CONTEXT *pSocketContext);
	//���ͻ���socket����Ϣ���������Ƴ�
	void _RemoveContext(PER_SOCKET_CONTEXT *pSocketContext);
	// ��տͻ���socket��Ϣ
	void _ClearContextList();

	// ������󶨵���ɶ˿���
	bool _AssociateWithIOCP(PER_SOCKET_CONTEXT *pContext);

	// ������ɶ˿��ϵĴ���
	bool HandleError(PER_SOCKET_CONTEXT *pContext, const DWORD& dwErr);

	//�̺߳�����ΪIOCP�������Ĺ������߳�
	static DWORD WINAPI _WorkerThread(LPVOID lpParam);

	//��ñ����Ĵ���������
	int _GetNoOfProcessors();

	//�жϿͻ���Socket�Ƿ��Ѿ��Ͽ�
	bool _IsSocketAlive(SOCKET s);

private:

	HANDLE                       m_hShutdownEvent;              // ����֪ͨ�߳�ϵͳ�˳����¼���Ϊ���ܹ����õ��˳��߳�

	HANDLE                       m_hIOCompletionPort;           // ��ɶ˿ڵľ��

	HANDLE*                      m_phWorkerThreads;             // �������̵߳ľ��ָ��

	int		                     m_nThreads;                    // ���ɵ��߳�����

	std::string                  m_strIP;                       // �������˵�IP��ַ
	int                          m_nPort;                       // �������˵ļ����˿�

	CRITICAL_SECTION             m_csContextList;               // ����Worker�߳�ͬ���Ļ�����

	std::list<PER_SOCKET_CONTEXT*>  m_arrayClientContext;          // �ͻ���Socket��Context��Ϣ        

	PER_SOCKET_CONTEXT*          m_pListenContext;              // ���ڼ�����Socket��Context��Ϣ

	LPFN_ACCEPTEX                m_lpfnAcceptEx;                // AcceptEx �� GetAcceptExSockaddrs �ĺ���ָ�룬���ڵ�����������չ����
	LPFN_GETACCEPTEXSOCKADDRS    m_lpfnGetAcceptExSockAddrs;
};


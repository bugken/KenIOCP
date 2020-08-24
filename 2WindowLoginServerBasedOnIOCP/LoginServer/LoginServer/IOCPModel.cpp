#include "IOCPModel.h"
#include "logger.h"

// ÿһ���������ϲ������ٸ��߳�
#define WORKER_THREADS_PER_PROCESSOR 2
// ͬʱͶ�ݵ�AcceptEx���������
#define MAX_POST_ACCEPT              10
// ���ݸ�Worker�̵߳��˳��ź�
#define EXIT_CODE                    NULL


// �ͷ�ָ��;����Դ�ĺ�

// �ͷ�ָ���
#define RELEASE(x)                      {if(x != NULL ){delete x;x=NULL;}}
// �ͷž����
#define RELEASE_HANDLE(x)               {if(x != NULL && x!=INVALID_HANDLE_VALUE){ CloseHandle(x);x = NULL;}}
// �ͷ�Socket��
#define RELEASE_SOCKET(x)               {if(x !=INVALID_SOCKET) { closesocket(x);x=INVALID_SOCKET;}}



CIOCPModel::CIOCPModel(void) :
	m_nThreads(0),
	m_hShutdownEvent(NULL),
	m_hIOCompletionPort(NULL),
	m_phWorkerThreads(NULL),
	m_strIP(DEFAULT_IP),
	m_nPort(DEFAULT_PORT),
	m_lpfnAcceptEx(NULL),
	m_pListenContext(NULL)
{
}


CIOCPModel::~CIOCPModel(void)
{
	// ȷ����Դ�����ͷ�
	this->Stop();
}


/*********************************************************************
*�������ܣ��̺߳���������GetQueuedCompletionStatus����������д���
*����������lpParam��THREADPARAMS_WORKER����ָ�룻
*����˵����GetQueuedCompletionStatus��ȷ����ʱ��ʾĳ�����Ѿ���ɣ��ڶ�������lpNumberOfBytes��ʾ�����׽��ִ�����ֽ�����
����lpCompletionKey��lpOverlapped������Ҫ����Ϣ�����ѯMSDN�ĵ���
*********************************************************************/
DWORD WINAPI CIOCPModel::_WorkerThread(LPVOID lpParam)
{
	THREADPARAMS_WORKER* pParam = (THREADPARAMS_WORKER*)lpParam;
	CIOCPModel* pIOCPModel = (CIOCPModel*)pParam->pIOCPModel;
	int nThreadNo = (int)pParam->nThreadNo;

	////INFO_LOGGER << "�������߳�������ID: " << nThreadNo << END_LOGGER;

	OVERLAPPED           *pOverlapped = NULL;
	PER_SOCKET_CONTEXT   *pSocketContext = NULL;
	DWORD                dwBytesTransfered = 0;

	//ѭ����������ֱ�����յ�Shutdown��ϢΪֹ
	while (WAIT_OBJECT_0 != WaitForSingleObject(pIOCPModel->m_hShutdownEvent, 0))
	{
		BOOL bReturn = GetQueuedCompletionStatus(
			pIOCPModel->m_hIOCompletionPort,
			&dwBytesTransfered,
			(PULONG_PTR)&pSocketContext,
			&pOverlapped,
			INFINITE);

		//����EXIT_CODE�˳���־����ֱ���˳�
		if (EXIT_CODE == (DWORD)pSocketContext)
		{
			break;
		}

		//����ֵΪ0����ʾ����
		if (!bReturn)
		{
			DWORD dwErr = GetLastError();

			// ��ʾһ����ʾ��Ϣ
			if (!pIOCPModel->HandleError(pSocketContext, dwErr))
			{
				break;
			}

			continue;
		}
		else
		{
			// ��ȡ����Ĳ���
			PER_IO_CONTEXT* pIoContext = CONTAINING_RECORD(pOverlapped, PER_IO_CONTEXT, m_Overlapped);

			// �ж��Ƿ��пͻ��˶Ͽ���
			if ((0 == dwBytesTransfered) && (RECV_POSTED == pIoContext->m_OpType || SEND_POSTED == pIoContext->m_OpType))
			{
				//INFO_LOGGER << "�ͻ��� " << inet_ntoa(pSocketContext->m_ClientAddr.sin_addr) << ":" << ntohs(pSocketContext->m_ClientAddr.sin_port) << " �Ͽ�����." << END_LOGGER;

				ERROR_LOGGER << "�ͻ���" <<  pIoContext->GetUserName() << "���˳�!" << END_LOGGER;

				// �ͷŵ���Ӧ����Դ
				pIOCPModel->_RemoveContext(pSocketContext);

				continue;
			}
			else
			{
				switch (pIoContext->m_OpType)
				{
					// Accept  
				case ACCEPT_POSTED:
				{
					pIoContext->m_nTotalBytes = dwBytesTransfered;
					pIOCPModel->_DoAccpet(pSocketContext, pIoContext);
				}
				break;

				// RECV
				case RECV_POSTED:
				{
					pIoContext->m_nTotalBytes = dwBytesTransfered;
					pIOCPModel->_DoRecv(pSocketContext, pIoContext);
				}
				break;

				case SEND_POSTED:
				{
					pIoContext->m_nSendBytes += dwBytesTransfered;
					if (pIoContext->m_nSendBytes < pIoContext->m_nTotalBytes)
					{
						//����δ�ܷ����꣬������������
						pIoContext->m_wsaBuf.buf = pIoContext->m_szBuffer + pIoContext->m_nSendBytes;
						pIoContext->m_wsaBuf.len = pIoContext->m_nTotalBytes - pIoContext->m_nSendBytes;
						pIOCPModel->PostWrite(pIoContext);
						//static int iCount = 1;
						//std::cout << "SEND_POSTED :" << iCount++ << std::endl;
					}
					else
					{
						pIOCPModel->PostRecv(pIoContext);
					}
				}
				break;
				default:
					// ��Ӧ��ִ�е�����
					ERROR_LOGGER << "_WorkThread�е� pIoContext->m_OpType �����쳣." << END_LOGGER;
					break;
				} //switch
			}//if
		}//if

	}//while

	//INFO_LOGGER << "�������߳� " << nThreadNo << "���˳�" << END_LOGGER;

	// �ͷ��̲߳���
	RELEASE(lpParam);

	return 0;
}

//�������ܣ���ʼ���׽���
bool CIOCPModel::LoadSocketLib()
{
	WSADATA wsaData;
	int nResult;
	nResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	// ����(һ�㶼�����ܳ���)
	if (NO_ERROR != nResult)
	{
		ERROR_LOGGER << "��ʼ��WinSock 2.2ʧ��!" << END_LOGGER;
		return false;
	}

	return true;
}


//�������ܣ�����������
bool CIOCPModel::Start()
{
	// ��ʼ���̻߳�����
	InitializeCriticalSection(&m_csContextList);

	// ����ϵͳ�˳����¼�֪ͨ
	m_hShutdownEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	// ��ʼ��IOCP
	if (false == _InitializeIOCP())
	{
		ERROR_LOGGER << "��ʼ��IOCPʧ�ܣ�" << END_LOGGER;
		return false;
	}
	else
	{
		//INFO_LOGGER << "IOCP��ʼ�����" << END_LOGGER;
	}

	// ��ʼ��Socket
	if (false == _InitializeListenSocket())
	{
		ERROR_LOGGER << "Listen Socket��ʼ��ʧ�ܣ�" << END_LOGGER;
		this->_DeInitialize();
		return false;
	}
	else
	{
		//INFO_LOGGER << "Listen Socket��ʼ�����." << END_LOGGER;
	}

	//INFO_LOGGER << "ϵͳ׼���������Ⱥ�����...." << END_LOGGER;

	return true;
}


////////////////////////////////////////////////////////////////////
//	��ʼ����ϵͳ�˳���Ϣ���˳���ɶ˿ں��߳���Դ
void CIOCPModel::Stop()
{
	if (m_pListenContext != NULL && m_pListenContext->m_Socket != INVALID_SOCKET)
	{
		// ����ر���Ϣ֪ͨ
		SetEvent(m_hShutdownEvent);

		for (int i = 0; i < m_nThreads; i++)
		{
			// ֪ͨ���е���ɶ˿ڲ����˳�
			PostQueuedCompletionStatus(m_hIOCompletionPort, 0, (DWORD)EXIT_CODE, NULL);
		}

		// �ȴ����еĿͻ�����Դ�˳�
		WaitForMultipleObjects(m_nThreads, m_phWorkerThreads, TRUE, INFINITE);

		// ����ͻ����б���Ϣ
		this->_ClearContextList();

		// �ͷ�������Դ
		this->_DeInitialize();

		//INFO_LOGGER << "ֹͣ����" << END_LOGGER;
	}
}


/*************************************************************
*�������ܣ�Ͷ��WSARecv����
*����������
PER_IO_CONTEXT* pIoContext:	���ڽ���IO���׽����ϵĽṹ����ҪΪWSARecv������WSASend������
**************************************************************/
bool CIOCPModel::PostRecv(PER_IO_CONTEXT* pIoContext)
{
	// ��ʼ������
	DWORD dwFlags = 0;
	DWORD dwBytes = 0;
	WSABUF *p_wbuf = &pIoContext->m_wsaBuf;
	OVERLAPPED *p_ol = &pIoContext->m_Overlapped;

	pIoContext->ResetBuffer();
	pIoContext->m_OpType = RECV_POSTED;
	pIoContext->m_nSendBytes = 0;
	pIoContext->m_nTotalBytes = 0;

	// ��ʼ����ɺ󣬣�Ͷ��WSARecv����
	int nBytesRecv = WSARecv(pIoContext->m_sockAccept, p_wbuf, 1, &dwBytes, &dwFlags, p_ol, NULL);

	// �������ֵ���󣬲��Ҵ���Ĵ��벢����Pending�Ļ����Ǿ�˵������ص�����ʧ����
	int iErr = WSAGetLastError();
	if ((SOCKET_ERROR == nBytesRecv) && (WSA_IO_PENDING != iErr))
	{
		ERROR_LOGGER << "Ͷ�ݵ�һ��WSARecvʧ��[" << iErr << "]" << END_LOGGER;
		return false;
	}

	return true;
}

/*************************************************************
*�������ܣ�Ͷ��WSASend����
*����������
PER_IO_CONTEXT* pIoContext:	���ڽ���IO���׽����ϵĽṹ����ҪΪWSARecv������WSASend����
*����˵��������PostWrite֮ǰ��Ҫ����pIoContext��m_wsaBuf, m_nTotalBytes, m_nSendBytes��
**************************************************************/
bool CIOCPModel::PostWrite(PER_IO_CONTEXT* pIoContext)
{
	// ��ʼ������
	DWORD dwFlags = 0;
	DWORD dwSendNumBytes = 0;
	WSABUF *p_wbuf = &pIoContext->m_wsaBuf;
	OVERLAPPED *p_ol = &pIoContext->m_Overlapped;

	//ҵ����
	pIoContext->m_pClientParse->Run(p_wbuf->buf);
	pIoContext->m_pClientParse->GetReturnValue(pIoContext->m_wsaBuf.buf, MAX_BUFFER_LEN, pIoContext->m_wsaBuf.len);

	//�����ܹ���Ҫ���͵��ֽ���
	pIoContext->m_nTotalBytes = pIoContext->m_wsaBuf.len;
	pIoContext->m_OpType = SEND_POSTED;

	//Ͷ��WSASend���� -- ��Ҫ�޸�
	int nRet = WSASend(pIoContext->m_sockAccept, &pIoContext->m_wsaBuf, 1, &dwSendNumBytes, dwFlags,
		&pIoContext->m_Overlapped, NULL);

	// �������ֵ���󣬲��Ҵ���Ĵ��벢����Pending�Ļ����Ǿ�˵������ص�����ʧ����
	if ((SOCKET_ERROR == nRet) && (WSA_IO_PENDING != WSAGetLastError()))
	{
		ERROR_LOGGER << "Ͷ��WSASendʧ�ܣ�" << END_LOGGER;
		return false;
	}
	return true;
}


////////////////////////////////
// ��ʼ����ɶ˿�
bool CIOCPModel::_InitializeIOCP()
{
	// ������һ����ɶ˿�
	m_hIOCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

	if (NULL == m_hIOCompletionPort)
	{
		ERROR_LOGGER << "������ɶ˿�ʧ�ܣ��������: " << WSAGetLastError() << END_LOGGER;
		return false;
	}

	// ���ݱ����еĴ�����������������Ӧ���߳���
	m_nThreads = WORKER_THREADS_PER_PROCESSOR * _GetNoOfProcessors();

	// Ϊ�������̳߳�ʼ�����
	m_phWorkerThreads = new HANDLE[m_nThreads];

	// ���ݼ�����������������������߳�
	DWORD nThreadID;
	for (int i = 0; i < m_nThreads; i++)
	{
		THREADPARAMS_WORKER* pThreadParams = new THREADPARAMS_WORKER;
		pThreadParams->pIOCPModel = this;
		pThreadParams->nThreadNo = i + 1;
		m_phWorkerThreads[i] = ::CreateThread(0, 0, _WorkerThread, (void *)pThreadParams, 0, &nThreadID);
	}

	//INFO_LOGGER << "���� _WorkerThread " << m_nThreads << "��" << END_LOGGER;

	return true;
}


/////////////////////////////////////////////////////////////////
// ��ʼ��Socket
bool CIOCPModel::_InitializeListenSocket()
{
	// AcceptEx �� GetAcceptExSockaddrs ��GUID�����ڵ�������ָ��
	GUID GuidAcceptEx = WSAID_ACCEPTEX;
	GUID GuidGetAcceptExSockAddrs = WSAID_GETACCEPTEXSOCKADDRS;

	// ��������ַ��Ϣ�����ڰ�Socket
	struct sockaddr_in ServerAddress;

	// �������ڼ�����Socket����Ϣ
	m_pListenContext = new PER_SOCKET_CONTEXT;

	// ��Ҫʹ���ص�IO�������ʹ��WSASocket������Socket���ſ���֧���ص�IO����
	m_pListenContext->m_Socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (INVALID_SOCKET == m_pListenContext->m_Socket)
	{
		ERROR_LOGGER << "��ʼ��Socketʧ�ܣ��������: " << WSAGetLastError() << END_LOGGER;
		return false;
	}
	else
	{
		ERROR_LOGGER << "WSASocket() ���." << END_LOGGER;
	}

	// ��Listen Socket������ɶ˿���
	if (NULL == CreateIoCompletionPort((HANDLE)m_pListenContext->m_Socket, m_hIOCompletionPort, (DWORD)m_pListenContext, 0))
	{
		ERROR_LOGGER << "�� Listen Socket����ɶ˿�ʧ�ܣ��������: " << WSAGetLastError() << END_LOGGER;
		RELEASE_SOCKET(m_pListenContext->m_Socket);
		return false;
	}
	else
	{
		//INFO_LOGGER << "Listen Socket����ɶ˿� ���." << END_LOGGER;
	}

	// ����ַ��Ϣ
	ZeroMemory((char *)&ServerAddress, sizeof(ServerAddress));
	ServerAddress.sin_family = AF_INET;
	// ������԰��κο��õ�IP��ַ�����߰�һ��ָ����IP��ַ 
	//ServerAddress.sin_addr.s_addr = htonl(INADDR_ANY);                      
	ServerAddress.sin_addr.s_addr = inet_addr(m_strIP.c_str());
	ServerAddress.sin_port = htons(m_nPort);

	// �󶨵�ַ�Ͷ˿�
	if (SOCKET_ERROR == bind(m_pListenContext->m_Socket, (struct sockaddr *) &ServerAddress, sizeof(ServerAddress)))
	{
		ERROR_LOGGER << "bind()����ִ�д���." << END_LOGGER;
		return false;
	}
	else
	{
		//INFO_LOGGER << "bind() ���." << END_LOGGER;
	}

	// ��ʼ���м���
	if (SOCKET_ERROR == listen(m_pListenContext->m_Socket, SOMAXCONN))
	{
		ERROR_LOGGER << "Listen()����ִ�г��ִ���." << END_LOGGER;
		return false;
	}
	else
	{
		//INFO_LOGGER << "Listen() ���." << END_LOGGER;
	}

	// ʹ��AcceptEx��������Ϊ���������WinSock2�淶֮���΢�������ṩ����չ����
	// ������Ҫ�����ȡһ�º�����ָ�룬
	// ��ȡAcceptEx����ָ��
	DWORD dwBytes = 0;
	if (SOCKET_ERROR == WSAIoctl(
		m_pListenContext->m_Socket,
		SIO_GET_EXTENSION_FUNCTION_POINTER,
		&GuidAcceptEx,
		sizeof(GuidAcceptEx),
		&m_lpfnAcceptEx,
		sizeof(m_lpfnAcceptEx),
		&dwBytes,
		NULL,
		NULL))
	{
		ERROR_LOGGER << "WSAIoctl δ�ܻ�ȡAcceptEx����ָ�롣�������: " << WSAGetLastError() << END_LOGGER;
		this->_DeInitialize();
		return false;
	}

	// ��ȡGetAcceptExSockAddrs����ָ�룬Ҳ��ͬ��
	if (SOCKET_ERROR == WSAIoctl(
		m_pListenContext->m_Socket,
		SIO_GET_EXTENSION_FUNCTION_POINTER,
		&GuidGetAcceptExSockAddrs,
		sizeof(GuidGetAcceptExSockAddrs),
		&m_lpfnGetAcceptExSockAddrs,
		sizeof(m_lpfnGetAcceptExSockAddrs),
		&dwBytes,
		NULL,
		NULL))
	{
		ERROR_LOGGER << "WSAIoctl δ�ܻ�ȡGuidGetAcceptExSockAddrs����ָ�롣�������: " << WSAGetLastError() << END_LOGGER;
		this->_DeInitialize();
		return false;
	}


	// ΪAcceptEx ׼��������Ȼ��Ͷ��AcceptEx I/O����
	//����10���׽��֣�Ͷ��AcceptEx���󣬼�����10���׽��ֽ���accept������
	for (int i = 0; i < MAX_POST_ACCEPT; i++)
	{
		// �½�һ��IO_CONTEXT
		PER_IO_CONTEXT* pAcceptIoContext = m_pListenContext->GetNewIoContext();

		if (false == this->_PostAccept(pAcceptIoContext))
		{
			m_pListenContext->RemoveContext(pAcceptIoContext);
			return false;
		}
	}

	//INFO_LOGGER << "Ͷ�� " << MAX_POST_ACCEPT << " ��AcceptEx�������" << END_LOGGER;

	return true;
}

////////////////////////////////////////////////////////////
//	����ͷŵ�������Դ
void CIOCPModel::_DeInitialize()
{
	// ɾ���ͻ����б�Ļ�����
	DeleteCriticalSection(&m_csContextList);

	// �ر�ϵͳ�˳��¼����
	RELEASE_HANDLE(m_hShutdownEvent);

	// �ͷŹ������߳̾��ָ��
	for (int i = 0; i < m_nThreads; i++)
	{
		RELEASE_HANDLE(m_phWorkerThreads[i]);
	}

	RELEASE(m_phWorkerThreads);

	// �ر�IOCP���
	RELEASE_HANDLE(m_hIOCompletionPort);

	// �رռ���Socket
	RELEASE(m_pListenContext);

	//INFO_LOGGER << "�ͷ���Դ���" << END_LOGGER;
}


//====================================================================================
//
//				    Ͷ����ɶ˿�����
//
//====================================================================================


//////////////////////////////////////////////////////////////////
// Ͷ��Accept����
bool CIOCPModel::_PostAccept(PER_IO_CONTEXT* pAcceptIoContext)
{
	if (INVALID_SOCKET == m_pListenContext->m_Socket)
	{
		ERROR_LOGGER << "INVALID_SOCKET == m_pListenContext->m_Socket, ����Ϊ��" << pAcceptIoContext->m_wsaBuf.buf << END_LOGGER;
		return false;
	}

	// ׼������
	DWORD dwBytes = 0;
	pAcceptIoContext->m_OpType = ACCEPT_POSTED;
	WSABUF *p_wbuf = &pAcceptIoContext->m_wsaBuf;
	OVERLAPPED *p_ol = &pAcceptIoContext->m_Overlapped;

	// Ϊ�Ժ�������Ŀͻ�����׼����Socket( ������봫ͳaccept�������� ) 
	pAcceptIoContext->m_sockAccept = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (INVALID_SOCKET == pAcceptIoContext->m_sockAccept)
	{
		ERROR_LOGGER << "��������Accept��Socketʧ�ܣ��������: " << WSAGetLastError() << END_LOGGER;
		return false;
	}

	//���ø��׽��ַ�����
	unsigned long ul = 1;
	int nRet = ioctlsocket(pAcceptIoContext->m_sockAccept, FIONBIO, (unsigned long*)&ul);
	if (nRet == SOCKET_ERROR)
	{
		//�����׽��ַ�����ģʽ��ʧ�ܴ����ȼǸ���־
		ERROR_LOGGER << "�����׽�������ģʽʧ��[" << WSAGetLastError() << "]" << END_LOGGER;
	}

	// Ͷ��AcceptEx
	if (FALSE == m_lpfnAcceptEx(m_pListenContext->m_Socket, pAcceptIoContext->m_sockAccept, p_wbuf->buf, p_wbuf->len - ((sizeof(SOCKADDR_IN) + 16) * 2),
		sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, &dwBytes, p_ol))
	{
		if (WSA_IO_PENDING != WSAGetLastError())
		{
			ERROR_LOGGER << "Ͷ�� AcceptEx ����ʧ�ܣ��������: " << WSAGetLastError() << END_LOGGER;
			return false;
		}
	}

	return true;
}


/********************************************************************
*�������ܣ��������пͻ��˽��봦��
*����˵����
PER_SOCKET_CONTEXT* pSocketContext:	����accept������Ӧ���׽��֣����׽�������Ӧ�����ݽṹ��
PER_IO_CONTEXT* pIoContext:			����accept������Ӧ�����ݽṹ��
DWORD		dwIOSize:				���β�������ʵ�ʴ�����ֽ���
********************************************************************/
bool CIOCPModel::_DoAccpet(PER_SOCKET_CONTEXT* pSocketContext, PER_IO_CONTEXT* pIoContext)
{

	if (pIoContext->m_nTotalBytes > 0)
	{
		//�ͻ�����ʱ����һ�ν���dwIOSize�ֽ�����
		_DoFirstRecvWithData(pIoContext);
	}
	else
	{
		//�ͻ��˽���ʱ��û�з������ݣ���Ͷ��WSARecv���󣬽�������
		_DoFirstRecvWithoutData(pIoContext);

	}

	// 5. ʹ�����֮�󣬰�Listen Socket���Ǹ�IoContext���ã�Ȼ��׼��Ͷ���µ�AcceptEx
	pIoContext->ResetBuffer();
	return this->_PostAccept(pIoContext);
}


/////////////////////////////////////////////////////////////////
//�������ܣ����н��յ����ݵ����ʱ�򣬽��д���
bool CIOCPModel::_DoRecv(PER_SOCKET_CONTEXT* pSocketContext, PER_IO_CONTEXT* pIoContext)
{
	//������յ�����
	SOCKADDR_IN* ClientAddr = &pSocketContext->m_ClientAddr;
	//INFO_LOGGER << "�յ�  " << inet_ntoa(ClientAddr->sin_addr) << ":" << ntohs(ClientAddr->sin_port) << " ��Ϣ��" << pIoContext->m_wsaBuf.buf << END_LOGGER;
	//std::cout << "�յ�  " << inet_ntoa(ClientAddr->sin_addr) << ":" << ntohs(ClientAddr->sin_port) << " ��Ϣ��" << pIoContext->m_wsaBuf.buf << std::endl;

	//static int iCount = 1;
	//std::cout << "_DoRecv :" << iCount++ << std::endl;

	//��������
	pIoContext->m_nSendBytes = 0;
	pIoContext->m_nTotalBytes = pIoContext->m_nTotalBytes;
	pIoContext->m_wsaBuf.len = pIoContext->m_nTotalBytes;
	pIoContext->m_wsaBuf.buf = pIoContext->m_szBuffer;
	return PostWrite(pIoContext);
}

/*************************************************************
*�������ܣ�AcceptEx���տͻ����ӳɹ������տͻ���һ�η��͵����ݣ���Ͷ��WSASend����
*����������
PER_IO_CONTEXT* pIoContext:	���ڼ����׽����ϵĲ���
**************************************************************/
bool CIOCPModel::_DoFirstRecvWithData(PER_IO_CONTEXT* pIoContext)
{
	SOCKADDR_IN* ClientAddr = NULL;
	SOCKADDR_IN* LocalAddr = NULL;
	int remoteLen = sizeof(SOCKADDR_IN), localLen = sizeof(SOCKADDR_IN);

	//1. ����ȡ������ͻ��˵ĵ�ַ��Ϣ
	this->m_lpfnGetAcceptExSockAddrs(pIoContext->m_wsaBuf.buf, pIoContext->m_wsaBuf.len - ((sizeof(SOCKADDR_IN) + 16) * 2),
		sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, (LPSOCKADDR*)&LocalAddr, &localLen, (LPSOCKADDR*)&ClientAddr, &remoteLen);

	//��ʾ�ͻ�����Ϣ
	//INFO_LOGGER << "�ͻ���  " << inet_ntoa(ClientAddr->sin_addr) << ":" << ntohs(ClientAddr->sin_port) << " ��Ϣ��" << pIoContext->m_wsaBuf.buf << END_LOGGER;

	//2.Ϊ�½�����׽Ӵ���PER_SOCKET_CONTEXT���������׽��ְ󶨵���ɶ˿�
	PER_SOCKET_CONTEXT* pNewSocketContext = new PER_SOCKET_CONTEXT;
	pNewSocketContext->m_Socket = pIoContext->m_sockAccept;
	memcpy(&(pNewSocketContext->m_ClientAddr), ClientAddr, sizeof(SOCKADDR_IN));

	// ����������ϣ������Socket����ɶ˿ڰ�(��Ҳ��һ���ؼ�����)
	if (false == this->_AssociateWithIOCP(pNewSocketContext))
	{
		RELEASE(pNewSocketContext);
		return false;
	}

	//3. �������������µ�IoContext�����������Socket��Ͷ�ݵ�һ��Recv��������
	PER_IO_CONTEXT* pNewIoContext = pNewSocketContext->GetNewIoContext();
	pNewIoContext->m_OpType = SEND_POSTED;
	pNewIoContext->m_sockAccept = pNewSocketContext->m_Socket;
	pNewIoContext->m_nTotalBytes = pIoContext->m_nTotalBytes;
	pNewIoContext->m_nSendBytes = 0;
	pNewIoContext->m_wsaBuf.len = pIoContext->m_nTotalBytes;
	memcpy(pNewIoContext->m_wsaBuf.buf, pIoContext->m_wsaBuf.buf, pIoContext->m_nTotalBytes);	//�������ݵ�WSASend�����Ĳ���������

	//static int iCount = 1;
	//std::cout << "_DoFirstRecvWithData :" << iCount++ << std::endl;
	//��ʱ�ǵ�һ�ν������ݳɹ�����������Ͷ��PostWrite����ͻ��˷�������
	if (false == this->PostWrite(pNewIoContext))
	{
		pNewSocketContext->RemoveContext(pNewIoContext);
		return false;
	}

	//4. ���Ͷ�ݳɹ�����ô�Ͱ������Ч�Ŀͻ�����Ϣ�����뵽ContextList��ȥ(��Ҫͳһ���������ͷ���Դ)
	this->_AddToContextList(pNewSocketContext);

	return true;
}

/*************************************************************
*�������ܣ�AcceptEx���տͻ����ӳɹ�����ʱ��δ���յ����ݣ���Ͷ��WSARecv����
*����������
PER_IO_CONTEXT* pIoContext:	���ڼ����׽����ϵĲ���
**************************************************************/
bool CIOCPModel::_DoFirstRecvWithoutData(PER_IO_CONTEXT* pIoContext)
{
	//Ϊ�½�����׽��ִ���PER_SOCKET_CONTEXT�ṹ�����󶨵���ɶ˿�
	PER_SOCKET_CONTEXT* pNewSocketContext = new PER_SOCKET_CONTEXT;
	SOCKADDR_IN ClientAddr;
	int Len = sizeof(ClientAddr);

	getpeername(pIoContext->m_sockAccept, (sockaddr*)&ClientAddr, &Len);

	pNewSocketContext->m_Socket = pIoContext->m_sockAccept;
	memcpy(&(pNewSocketContext->m_ClientAddr), &ClientAddr, sizeof(SOCKADDR_IN));

	//�����׽��ְ󶨵���ɶ˿�
	if (false == this->_AssociateWithIOCP(pNewSocketContext))
	{
		RELEASE(pNewSocketContext);
		return false;
	}

	//Ͷ��WSARecv���󣬽�������
	PER_IO_CONTEXT* pNewIoContext = pNewSocketContext->GetNewIoContext();

	//��ʱ��AcceptExδ���յ��ͻ��˵�һ�η��͵����ݣ������������PostRecv���������Կͻ��˵�����
	if (false == this->PostRecv(pNewIoContext))
	{
		pNewSocketContext->RemoveContext(pNewIoContext);
		return false;
	}

	//���Ͷ�ݳɹ�����ô�Ͱ������Ч�Ŀͻ�����Ϣ�����뵽ContextList��ȥ(��Ҫͳһ���������ͷ���Դ)
	this->_AddToContextList(pNewSocketContext);

	return true;
}


/////////////////////////////////////////////////////
// �����(Socket)�󶨵���ɶ˿���
bool CIOCPModel::_AssociateWithIOCP(PER_SOCKET_CONTEXT *pContext)
{
	// �����ںͿͻ���ͨ�ŵ�SOCKET�󶨵���ɶ˿���
	HANDLE hTemp = CreateIoCompletionPort((HANDLE)pContext->m_Socket, m_hIOCompletionPort, (DWORD)pContext, 0);

	if (NULL == hTemp)
	{
		ERROR_LOGGER << "ִ��CreateIoCompletionPort()���ִ���.������룺" << WSAGetLastError() << END_LOGGER;
		return false;
	}

	return true;
}



//////////////////////////////////////////////////////////////
// ���ͻ��˵������Ϣ�洢��������
void CIOCPModel::_AddToContextList(PER_SOCKET_CONTEXT *pHandleData)
{
	EnterCriticalSection(&m_csContextList);

	//m_arrayClientContext.Add(pHandleData);

	LeaveCriticalSection(&m_csContextList);
}

////////////////////////////////////////////////////////////////
//	�Ƴ�ĳ���ض���Context
void CIOCPModel::_RemoveContext(PER_SOCKET_CONTEXT *pSocketContext)
{
	EnterCriticalSection(&m_csContextList);

	for (auto it = m_arrayClientContext.begin(); it != m_arrayClientContext.end(); it++)
	{
		if (pSocketContext == *it)
		{
			RELEASE(pSocketContext);
			m_arrayClientContext.remove(pSocketContext);
			break;
		}
	}

	LeaveCriticalSection(&m_csContextList);
}

////////////////////////////////////////////////////////////////
// ��տͻ�����Ϣ
void CIOCPModel::_ClearContextList()
{
	EnterCriticalSection(&m_csContextList);

	for (auto it = m_arrayClientContext.begin(); it != m_arrayClientContext.end(); it++)
	{
		delete *it;
	}
	m_arrayClientContext.clear();

	LeaveCriticalSection(&m_csContextList);
}



////////////////////////////////////////////////////////////////////
// ��ñ�����IP��ַ
std::string CIOCPModel::GetLocalIP()
{
	// ��ñ���������
	char hostname[MAX_PATH] = { 0 };
	gethostname(hostname, MAX_PATH);
	struct hostent FAR* lpHostEnt = gethostbyname(hostname);
	if (lpHostEnt == NULL)
	{
		return DEFAULT_IP;
	}

	// ȡ��IP��ַ�б��еĵ�һ��Ϊ���ص�IP(��Ϊһ̨�������ܻ�󶨶��IP)
	LPSTR lpAddr = lpHostEnt->h_addr_list[0];

	// ��IP��ַת�����ַ�����ʽ
	struct in_addr inAddr;
	memmove(&inAddr, lpAddr, 4);
	m_strIP = std::string(inet_ntoa(inAddr));

	return m_strIP;
}

///////////////////////////////////////////////////////////////////
// ��ñ����д�����������
int CIOCPModel::_GetNoOfProcessors()
{
	SYSTEM_INFO si;

	GetSystemInfo(&si);

	return si.dwNumberOfProcessors;
}

/////////////////////////////////////////////////////////////////////
// �жϿͻ���Socket�Ƿ��Ѿ��Ͽ���������һ����Ч��Socket��Ͷ��WSARecv����������쳣
// ʹ�õķ����ǳ��������socket�������ݣ��ж����socket���õķ���ֵ
// ��Ϊ����ͻ��������쳣�Ͽ�(����ͻ��˱������߰ε����ߵ�)��ʱ�򣬷����������޷��յ��ͻ��˶Ͽ���֪ͨ��

bool CIOCPModel::_IsSocketAlive(SOCKET s)
{
	int nByteSent = send(s, "", 0, 0);
	if (-1 == nByteSent)
		return false;
	return true;
}

///////////////////////////////////////////////////////////////////
//�������ܣ���ʾ��������ɶ˿��ϵĴ���
bool CIOCPModel::HandleError(PER_SOCKET_CONTEXT *pContext, const DWORD& dwErr)
{
	// ����ǳ�ʱ�ˣ����ټ����Ȱ�  
	if (WAIT_TIMEOUT == dwErr)
	{
		// ȷ�Ͽͻ����Ƿ񻹻���...
		if (!_IsSocketAlive(pContext->m_Socket))
		{
			ERROR_LOGGER << "��⵽�ͻ����쳣�˳�!" << END_LOGGER;
			this->_RemoveContext(pContext);
			return true;
		}
		else
		{
			//WARNNING_LOGGER << "���������ʱ��������..." << END_LOGGER;
			return true;
		}
	}

	// �����ǿͻ����쳣�˳���
	else if (ERROR_NETNAME_DELETED == dwErr)
	{
		ERROR_LOGGER << "�ͻ������˳�!" << END_LOGGER;
		this->_RemoveContext(pContext);
		return true;
	}

	else
	{
		ERROR_LOGGER << "��ɶ˿ڲ������ִ����߳��˳���������룺" << (int)dwErr << END_LOGGER;
		return false;
	}
}





#include "Network.h"
#include "LockGuard.h"
#include "PerIoContext.h"
#include "PerSocketContext.h"
#include "IocpServer.h"
#include <assert.h>
#include <process.h>
#include <mswsock.h>
//for struct tcp_keepalive
#include <mstcpip.h>
#include <thread>
#include <iostream>
using namespace std;

//�����߳��˳���־
#define EXIT_THREAD 0
#define POST_ACCEPT_CNT 10

IocpServer::IocpServer(short listenPort, int maxConnectionCount) :
	m_bIsShutdown(false)
	, m_hComPort(NULL)
	, m_hExitEvent(NULL)
	, m_hWriteCompletedEvent(NULL)
	, m_listenPort(listenPort)
	, m_pListenCtx(nullptr)
	, m_nWorkerCnt(0)
	, m_nConnClientCnt(0)
	, m_nMaxConnClientCnt(maxConnectionCount)
{
	//�ֶ�reset����ʼ״̬Ϊnonsignaled
	m_hExitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (WSA_INVALID_EVENT == m_hExitEvent)
	{
		cout << "CreateEvent failed with error: " << WSAGetLastError() << endl;
	}
	//�Զ�reset����ʼ״̬Ϊsignaled
	m_hWriteCompletedEvent = CreateEvent(NULL, FALSE, TRUE, NULL);
	if (WSA_INVALID_EVENT == m_hWriteCompletedEvent)
	{
		cout << "CreateEvent failed with error: " << WSAGetLastError() << endl;
	}
	InitializeCriticalSection(&m_csClientList);
}

IocpServer::~IocpServer()
{
	stop();
	DeleteCriticalSection(&m_csClientList);
	Network::unInit();
}

bool IocpServer::start()
{
	if (!Network::init())
	{
		cout << "network initial failed" << endl;
		return false;
	}
	if (!createListenClient(m_listenPort))
	{
		return false;
	}
	if (!createIocpWorker())
	{
		return false;
	}
	if (!initAcceptIoContext())
	{
		return false;
	}
	return true;
}

bool IocpServer::stop()
{
	//ͬ���ȴ����й����߳��˳�
	exitIocpWorker();
	//�رչ����߳̾��
	for_each(m_hWorkerThreads.begin(), m_hWorkerThreads.end(),
		[](const HANDLE& h) { CloseHandle(h); });
	for_each(m_acceptIoCtxList.begin(), m_acceptIoCtxList.end(),
		[](AcceptIoContext* mAcceptIoCtx) {
			CancelIo((HANDLE)mAcceptIoCtx->m_acceptSocket);
			closesocket(mAcceptIoCtx->m_acceptSocket);
			mAcceptIoCtx->m_acceptSocket = INVALID_SOCKET;
			while (!HasOverlappedIoCompleted(&mAcceptIoCtx->m_Overlapped))
			{
				Sleep(1);
			}
			delete mAcceptIoCtx;
		});
	m_acceptIoCtxList.clear();
	if (m_hExitEvent)
	{
		CloseHandle(m_hExitEvent);
		m_hExitEvent = NULL;
	}
	if (m_hComPort)
	{
		CloseHandle(m_hComPort);
		m_hComPort = NULL;
	}
	if (m_pListenCtx)
	{
		closesocket(m_pListenCtx->m_socket);
		m_pListenCtx->m_socket = INVALID_SOCKET;
		delete m_pListenCtx;
		m_pListenCtx = nullptr;
	}
	removeAllClients();
	return true;
}

bool IocpServer::shutdown()
{
	m_bIsShutdown = true;

	int ret = CancelIoEx((HANDLE)m_pListenCtx->m_socket, NULL);
	if (0 == ret)
	{
		cout << "CancelIoEx failed with error: " << WSAGetLastError() << endl;
		return false;
	}
	closesocket(m_pListenCtx->m_socket);
	m_pListenCtx->m_socket = INVALID_SOCKET;

	for_each(m_acceptIoCtxList.begin(), m_acceptIoCtxList.end(),
		[](AcceptIoContext* pAcceptIoCtx)
		{
			int ret = CancelIoEx((HANDLE)pAcceptIoCtx->m_acceptSocket, 
				&pAcceptIoCtx->m_Overlapped);
			if (0 == ret)
			{
				cout << "CancelIoEx failed with error: " << WSAGetLastError() << endl;
				return;
			}
			closesocket(pAcceptIoCtx->m_acceptSocket);
			pAcceptIoCtx->m_acceptSocket = INVALID_SOCKET;

			while (!HasOverlappedIoCompleted(&pAcceptIoCtx->m_Overlapped))
				Sleep(1);

			delete pAcceptIoCtx;
		});
	m_acceptIoCtxList.clear();

	return false;
}

bool IocpServer::send(ClientContext* pConnClient, PBYTE pData, UINT len)
{
	Buffer sendBuf;
	sendBuf.write(pData, len);
	LockGuard lk(&pConnClient->m_csLock);
	if (0 == pConnClient->m_outBuf.getBufferLen())
	{
		//��һ��Ͷ�ݣ�++m_nPendingIoCnt
		enterIoLoop(pConnClient);
		pConnClient->m_outBuf.copy(sendBuf);
		pConnClient->m_sendIoCtx->m_wsaBuf.buf = (PCHAR)pConnClient->m_outBuf.getBuffer();
		pConnClient->m_sendIoCtx->m_wsaBuf.len = pConnClient->m_outBuf.getBufferLen();

		PostResult result = postSend(pConnClient);
		if (PostResult::PostResultFailed == result)
		{
			CloseClient(pConnClient);
			releaseClientContext(pConnClient);
			return false;
		}
	}
	else
	{
		pConnClient->m_outBufQueue.push(sendBuf);
	}
	//int ret = WaitForSingleObject(m_hWriteCompletedEvent, INFINITE);
	//PostQueuedCompletionStatus(m_hComPort, 0, (ULONG_PTR)pConnClient,
	//	&pConnClient->m_sendIoCtx->m_overlapped);
	return true;
}

unsigned WINAPI IocpServer::IocpWorkerThread(LPVOID arg)
{
	IocpServer* pThis = static_cast<IocpServer*>(arg);
	LPOVERLAPPED    lpOverlapped = nullptr;
	ULONG_PTR       lpCompletionKey = 0;
	DWORD           dwMilliSeconds = INFINITE;
	DWORD           dwBytesTransferred;
	int             ret;

	while (WAIT_OBJECT_0 != WaitForSingleObject(pThis->m_hExitEvent, 0))
	{
		ret = GetQueuedCompletionStatus(pThis->m_hComPort, &dwBytesTransferred,
			&lpCompletionKey, &lpOverlapped, dwMilliSeconds);

		if (EXIT_THREAD == lpCompletionKey)
		{
			//�˳������߳�
			cout << "EXIT_THREAD" << endl;
			break;
		}
		// shutdown״̬��ֹͣ��������
		if (pThis->m_bIsShutdown && lpCompletionKey == (ULONG_PTR)pThis)
		{
			continue;
		}

		if (lpCompletionKey != (ULONG_PTR)pThis)
		{
			//�ĵ�˵��ʱ��ʱ�򴥷���INFINITE���ᴥ��
			//ʵ����curl������ctrl+cǿ�ƹر�����Ҳ�ᴥ��
			if (0 == ret)
			{
				cout << "GetQueuedCompletionStatus failed with error: "
					<< WSAGetLastError() << endl;
				pThis->handleClose(lpCompletionKey);
				continue;
			}

			//�Զ˹ر�
			if (0 == dwBytesTransferred)
			{
				pThis->handleClose(lpCompletionKey);
				continue;
			}
		}

		IoContext* pIoCtx = (IoContext*)lpOverlapped;
		switch (pIoCtx->m_PostType)
		{
		case PostType::ACCEPT:
			pThis->handleAccept(lpOverlapped, dwBytesTransferred);
			break;
		case PostType::RECV:
			pThis->handleRecv(lpCompletionKey, lpOverlapped, dwBytesTransferred);
			break;
		case PostType::SEND:
			pThis->handleSend(lpCompletionKey, lpOverlapped, dwBytesTransferred);
			break;
		default:
			break;
		}
	}

	cout << "exit" << endl;
	return 0;
}

HANDLE IocpServer::associateWithCompletionPort(SOCKET s, ULONG_PTR completionKey)
{
	HANDLE hRet;
	if (NULL == completionKey)
	{
		hRet = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	}
	else
	{
		hRet = CreateIoCompletionPort((HANDLE)s, m_hComPort, completionKey, 0);
	}
	if (NULL == hRet)
	{
		cout << "failed to associate the socket with completion port" << endl;
	}
	return hRet;
}

bool IocpServer::getAcceptExPtr()
{
	DWORD dwBytes;
	GUID GuidAcceptEx = WSAID_ACCEPTEX;
	LPFN_ACCEPTEX lpfnAcceptEx = NULL;
	int ret = WSAIoctl(m_pListenCtx->m_socket,
		SIO_GET_EXTENSION_FUNCTION_POINTER,
		&GuidAcceptEx, sizeof(GuidAcceptEx),
		&lpfnAcceptEx, sizeof(lpfnAcceptEx),
		&dwBytes, NULL, NULL);
	if (SOCKET_ERROR == ret)
	{
		cout << "WSAIoctl failed with error: " << WSAGetLastError();
		closesocket(m_pListenCtx->m_socket);
		return false;
	}
	m_lpfnAcceptEx = lpfnAcceptEx;
	return true;
}

bool IocpServer::getAcceptExSockaddrs()
{
	DWORD dwBytes;
	GUID GuidAddrs = WSAID_GETACCEPTEXSOCKADDRS;
	LPFN_GETACCEPTEXSOCKADDRS lpfnGetAcceptExAddr = NULL;
	int ret = WSAIoctl(m_pListenCtx->m_socket, 
		SIO_GET_EXTENSION_FUNCTION_POINTER,
		&GuidAddrs, sizeof(GuidAddrs),
		&lpfnGetAcceptExAddr, sizeof(lpfnGetAcceptExAddr),
		&dwBytes, NULL, NULL);
	if (SOCKET_ERROR == ret)
	{
		cout << "WSAIoctl failed with error: " << WSAGetLastError();
		closesocket(m_pListenCtx->m_socket);
		return false;
	}
	m_lpfnGetAcceptExAddr = lpfnGetAcceptExAddr;
	return true;
}

bool IocpServer::setKeepAlive(ClientContext* pConnClient,
	LPOVERLAPPED lpOverlapped, int time, int interval)
{
	if (!Network::setKeepAlive(pConnClient->m_socket, true))
		return false;

	//LPWSAOVERLAPPED pOl = &pConnClient->m_recvIoCtx->m_overlapped;
	//LPWSAOVERLAPPED pOl = nullptr;
	LPWSAOVERLAPPED pOl = lpOverlapped;

	tcp_keepalive keepAlive;
	keepAlive.onoff = 1;
	keepAlive.keepalivetime = time * 1000;
	keepAlive.keepaliveinterval = interval * 1000;
	DWORD dwBytes;
	//����msdn����Ҫ��һ��OVERLAPPED�ṹ
	int ret = WSAIoctl(pConnClient->m_socket, SIO_KEEPALIVE_VALS,
		&keepAlive, sizeof(tcp_keepalive), NULL, 0,
		&dwBytes, pOl, NULL);
	if (SOCKET_ERROR == ret && WSA_IO_PENDING != WSAGetLastError())
	{
		cout << "WSAIoctl failed with error: " << WSAGetLastError() << endl;
		return false;
	}
	return true;
}

bool IocpServer::createListenClient(short listenPort)
{
	m_pListenCtx = new ListenContext(listenPort);
	//������ɶ˿�
	m_hComPort = associateWithCompletionPort(INVALID_SOCKET, NULL);
	if (NULL == m_hComPort)
	{
		return false;
	}
	//��������socket����ɶ˿ڣ����ｫthisָ����ΪcompletionKey����ɶ˿�
	if (NULL == associateWithCompletionPort(m_pListenCtx->m_socket, (ULONG_PTR)this))
	{
		return false;
	}
	if (SOCKET_ERROR == Network::bind(m_pListenCtx->m_socket, &m_pListenCtx->m_addr))
	{
		cout << "bind failed" << endl;
		return false;
	}
	if (SOCKET_ERROR == Network::listen(m_pListenCtx->m_socket))
	{
		cout << "listen failed" << endl;
		return false;
	}
	//��ȡacceptEx����ָ��
	if (!getAcceptExPtr())
	{
		return false;
	}
	//��ȡGetAcceptExSockaddrs����ָ��
	if (!getAcceptExSockaddrs())
	{
		return false;
	}
	return true;
}

bool IocpServer::createIocpWorker()
{
	//����CPU��������IO�߳�
	HANDLE hWorker;
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	for (DWORD i = 0; i < sysInfo.dwNumberOfProcessors; ++i)
	{
		hWorker = (HANDLE)_beginthreadex(NULL, 0, IocpWorkerThread, this, 0, NULL);
		if (NULL == hWorker)
		{
			return false;
		}
		m_hWorkerThreads.emplace_back(hWorker);
		++m_nWorkerCnt;
	}
	cout << "started iocp worker thread count: " << m_nWorkerCnt << endl;
	return true;
}

bool IocpServer::exitIocpWorker()
{
	int ret = 0;
	SetEvent(m_hExitEvent);
	for (int i = 0; i < m_nWorkerCnt; ++i)
	{
		//֪ͨ�����߳��˳�
		ret = PostQueuedCompletionStatus(m_hComPort, 0, EXIT_THREAD, NULL);
		if (FALSE == ret)
		{
			cout << "PostQueuedCompletionStatus failed with error: "
				<< WSAGetLastError() << endl;
		}
	}
	//���ﲻ����Ϊʲô�᷵��0������Ӧ�÷���m_nWorkerCnt-1��
	ret = WaitForMultipleObjects(m_nWorkerCnt, m_hWorkerThreads.data(), TRUE, INFINITE);
	return true;
}

bool IocpServer::initAcceptIoContext()
{
	//Ͷ��accept����
	for (int i = 0; i < POST_ACCEPT_CNT; ++i)
	{
		AcceptIoContext* pAcceptIoCtx = new AcceptIoContext(PostType::ACCEPT);
		m_acceptIoCtxList.emplace_back(pAcceptIoCtx);
		if (!postAccept(pAcceptIoCtx))
		{
			return false;
		}
	}
	return true;
}

bool IocpServer::postAccept(AcceptIoContext* pAcceptIoCtx)
{
	pAcceptIoCtx->resetBuffer();

	DWORD dwRecvByte;
	//PCHAR pBuf = pAcceptIoCtx->m_wsaBuf.buf;
	//ULONG nLen = pAcceptIoCtx->m_wsaBuf.len - ACCEPT_ADDRS_SIZE;

	LPOVERLAPPED pOverlapped = &pAcceptIoCtx->m_Overlapped;
	LPFN_ACCEPTEX lpfnAcceptEx = (LPFN_ACCEPTEX)m_lpfnAcceptEx;

	//�������ڽ������ӵ�socket
	pAcceptIoCtx->m_acceptSocket = Network::socket();
	if (SOCKET_ERROR == pAcceptIoCtx->m_acceptSocket)
	{
		cout << "create socket failed" << endl;
		return false;
	}

	/*
	* ʹ��acceptEx��һ�����⣺
	* ����ͻ�������ȴû�������ݣ���acceptEx���ᴥ����ɰ������˷ѷ�������Դ
	* ���������Ϊ�˷�ֹ�������ӣ�accpetEx�������û����ݣ�
	* 	ֻ���յ�ַ��û�취���ӿڵ��ñ����ṩ��������
	*/
	static BYTE addrBuf[DOUBLE_ACCEPT_ADDRS_SIZE];
	if (FALSE == lpfnAcceptEx(m_pListenCtx->m_socket, pAcceptIoCtx->m_acceptSocket,
		addrBuf, 0, ACCEPT_ADDRS_SIZE, ACCEPT_ADDRS_SIZE,
		&dwRecvByte, pOverlapped))
	{
		if (WSA_IO_PENDING != WSAGetLastError())
		{
			cout << "acceptEx failed" << endl;
			return false;
		}
	}
	else
	{
		// Accept completed synchronously. We need to marshal �ռ�
		// the data received over to the worker thread ourselves...
	}
	return true;
}

PostResult IocpServer::postRecv(ClientContext* pConnClient)
{
	PostResult result = PostResult::PostResultSucc;
	RecvIoContext* pRecvIoCtx = pConnClient->m_recvIoCtx;

	pRecvIoCtx->resetBuffer();

	LockGuard lk(&pConnClient->m_csLock);
	if (INVALID_SOCKET != pConnClient->m_socket)
	{
		DWORD dwBytes;
		//���������־����û�����������һ�ν���
		DWORD dwFlag = MSG_PARTIAL;
		int ret = WSARecv(pConnClient->m_socket, &pRecvIoCtx->m_wsaBuf, 1,
			&dwBytes, &dwFlag, &pRecvIoCtx->m_Overlapped, NULL);
		if (SOCKET_ERROR == ret && WSA_IO_PENDING != WSAGetLastError())
		{
			cout << "WSARecv failed with error: " << WSAGetLastError() << endl;
			result = PostResult::PostResultFailed;
		}
	}
	else
	{
		result = PostResult::PostResultInvalidSocket;
	}
	return result;
}

PostResult IocpServer::postSend(ClientContext* pConnClient)
{
	PostResult result = PostResult::PostResultSucc;
	IoContext* pSendIoCtx = pConnClient->m_sendIoCtx;

	LockGuard lk(&pConnClient->m_csLock);
	if (INVALID_SOCKET != pConnClient->m_socket)
	{
		DWORD dwBytesSent;
		DWORD dwFlag = MSG_PARTIAL;
		int ret = WSASend(pConnClient->m_socket, &pSendIoCtx->m_wsaBuf, 1, &dwBytesSent,
			dwFlag, &pSendIoCtx->m_Overlapped, NULL);
		if (SOCKET_ERROR == ret && WSA_IO_PENDING != WSAGetLastError())
		{
			cout << "WSASend failed with error: " << WSAGetLastError() << endl;
			result = PostResult::PostResultFailed;
		}
	}
	else
	{
		result = PostResult::PostResultInvalidSocket;
	}
	return result;
}

bool IocpServer::handleAccept(LPOVERLAPPED lpOverlapped, DWORD dwBytesTransferred)
{
	AcceptIoContext* pAcceptIoCtx = (AcceptIoContext*)lpOverlapped;
	Network::updateAcceptContext(m_pListenCtx->m_socket, pAcceptIoCtx->m_acceptSocket);
	//�ﵽ�����������ر��µ�socket
	if (m_nConnClientCnt >= m_nMaxConnClientCnt)
	{
		closesocket(pAcceptIoCtx->m_acceptSocket);
		pAcceptIoCtx->m_acceptSocket = INVALID_SOCKET;
		postAccept(pAcceptIoCtx);
		return true;
	}
	InterlockedIncrement(&m_nConnClientCnt);	
	//�����µ�ClientContext��ԭ����IoContextҪ���������µ�����
	//ClientContext�մ������ڴ˺�������Ҫ����
	ClientContext* pConnClient = allocateClientContext(pAcceptIoCtx->m_acceptSocket);
	//memcpy_s(&pConnClient->m_addr, peerAddrLen, peerAddr, peerAddrLen);
	if (NULL == associateWithCompletionPort(pConnClient->m_socket,
		(ULONG_PTR)pConnClient))
	{
		return false;
	}
	enterIoLoop(pConnClient);
	//������������
	//setKeepAlive(pConnClient, &pAcceptIoCtx->m_overlapped);
	//pConnClient->appendToBuffer((PBYTE)pBuf, dwBytesTransferred);
	//Ͷ��һ���µ�accpet����
	postAccept(pAcceptIoCtx);
	notifyNewConnection(pConnClient);
	//notifyPackageReceived(pConnClient);
	//���ͻ��˼��������б�
	addClient(pConnClient);
	//Ͷ��recv����,����invalid socket�Ƿ�Ҫ�رտͻ��ˣ�
	PostResult result = postRecv(pConnClient);
	if (PostResult::PostResultFailed == result
		|| PostResult::PostResultInvalidSocket == result)
	{
		CloseClient(pConnClient);
		releaseClientContext(pConnClient);
	}
	return true;
}

bool IocpServer::handleRecv(ULONG_PTR lpCompletionKey,
	LPOVERLAPPED lpOverlapped, DWORD dwBytesTransferred)
{
	ClientContext* pConnClient = (ClientContext*)lpCompletionKey;
	RecvIoContext* pRecvIoCtx = (RecvIoContext*)lpOverlapped;
	pConnClient->appendToBuffer(pRecvIoCtx->m_recvBuf, dwBytesTransferred);
	notifyPackageReceived(pConnClient);

	//Ͷ��recv����
	PostResult result = postRecv(pConnClient);
	if (PostResult::PostResultFailed == result
		|| PostResult::PostResultInvalidSocket == result)
	{
		CloseClient(pConnClient);
		releaseClientContext(pConnClient);
	}
	return true;
}

bool IocpServer::handleSend(ULONG_PTR lpCompletionKey,
	LPOVERLAPPED lpOverlapped, DWORD dwBytesTransferred)
{
	ClientContext* pConnClient = (ClientContext*)lpCompletionKey;
	IoContext* pIoCtx = (IoContext*)lpOverlapped;
	DWORD n = -1;

	LockGuard lk(&pConnClient->m_csLock);
	pConnClient->m_outBuf.remove(dwBytesTransferred);
	if (0 == pConnClient->m_outBuf.getBufferLen())
	{
		notifyWriteCompleted();
		pConnClient->m_outBuf.clear();

		if (!pConnClient->m_outBufQueue.empty())
		{
			pConnClient->m_outBuf.copy(pConnClient->m_outBufQueue.front());
			pConnClient->m_outBufQueue.pop();
		}
		else
		{
			releaseClientContext(pConnClient);
		}
	}
	if (0 != pConnClient->m_outBuf.getBufferLen())
	{
		pIoCtx->m_wsaBuf.buf = (PCHAR)pConnClient->m_outBuf.getBuffer();
		pIoCtx->m_wsaBuf.len = pConnClient->m_outBuf.getBufferLen();

		PostResult result = postSend(pConnClient);
		if (PostResult::PostResultFailed == result)
		{
			CloseClient(pConnClient);
			releaseClientContext(pConnClient);
		}
	}
	return false;
}

bool IocpServer::handleClose(ULONG_PTR lpCompletionKey)
{
	ClientContext* pConnClient = (ClientContext*)lpCompletionKey;
	CloseClient(pConnClient);
	releaseClientContext(pConnClient);
	return true;
}

void IocpServer::enterIoLoop(ClientContext* pClientCtx)
{
	InterlockedIncrement(&pClientCtx->m_nPendingIoCnt);
}

int IocpServer::exitIoLoop(ClientContext* pClientCtx)
{
	return InterlockedDecrement(&pClientCtx->m_nPendingIoCnt);
}

void IocpServer::CloseClient(ClientContext* pConnClient)
{
	SOCKET s;
	Addr peerAddr;
	{
		LockGuard lk(&pConnClient->m_csLock);
		s = pConnClient->m_socket;
		peerAddr = pConnClient->m_addr;
		pConnClient->m_socket = INVALID_SOCKET;
	}
	if (INVALID_SOCKET != s)
	{
		notifyDisconnected(s, peerAddr);
		if (!Network::setLinger(s))
		{
			return;
		}
		int ret = CancelIoEx((HANDLE)s, NULL);
		//ERROR_NOT_FOUND : cannot find a request to cancel
		if (0 == ret && ERROR_NOT_FOUND != WSAGetLastError())
		{
			cout << "CancelIoEx failed with error: " 
				<< WSAGetLastError() << endl;
			return;
		}

		closesocket(s);
		InterlockedDecrement(&m_nConnClientCnt);
	}
}

void IocpServer::addClient(ClientContext* pConnClient)
{
	LockGuard lk(&m_csClientList);
	m_connectedClientList.emplace_back(pConnClient);
}

void IocpServer::removeClient(ClientContext* pConnClient)
{
	LockGuard lk(&m_csClientList);
	{
		auto it = std::find(m_connectedClientList.begin(),
			m_connectedClientList.end(), pConnClient);
		if (m_connectedClientList.end() != it)
		{
			m_connectedClientList.remove(pConnClient);
			while (!pConnClient->m_outBufQueue.empty())
			{
				pConnClient->m_outBufQueue.pop();
			}
			pConnClient->m_nPendingIoCnt = 0;
			m_freeClientList.emplace_back(pConnClient);
		}
	}
}

void IocpServer::removeAllClients()
{
	LockGuard lk(&m_csClientList);
	m_connectedClientList.erase(m_connectedClientList.begin(),
		m_connectedClientList.end());
}

ClientContext* IocpServer::allocateClientContext(SOCKET s)
{
	ClientContext* pClientCtx = nullptr;
	LockGuard lk(&m_csClientList);
	if (m_freeClientList.empty())
	{
		pClientCtx = new ClientContext(s);
	}
	else
	{
		pClientCtx = m_freeClientList.front();
		m_freeClientList.pop_front();
		pClientCtx->m_nPendingIoCnt = 0;
		pClientCtx->m_socket = s;
	}
	pClientCtx->reset();
	return pClientCtx;
}

void IocpServer::releaseClientContext(ClientContext* pConnClient)
{
	if (exitIoLoop(pConnClient) <= 0)
	{
		removeClient(pConnClient);
		//���ﲻɾ�������ǽ�ClientContext�Ƶ���������
		//delete pConnClient;
	}
}

void IocpServer::echo(ClientContext* pConnClient)
{
	send(pConnClient, pConnClient->m_inBuf.getBuffer(),
		pConnClient->m_inBuf.getBufferLen());
	pConnClient->m_inBuf.remove(pConnClient->m_inBuf.getBufferLen());
}

void IocpServer::notifyNewConnection(ClientContext* pConnClient)
{
	SOCKADDR_IN sockaddr = Network::getpeername(pConnClient->m_socket);
	pConnClient->m_addr = sockaddr;
	cout << "connected client: " << pConnClient->m_addr.toString()
		<< ", fd: " << pConnClient->m_socket << endl;
}

void IocpServer::notifyDisconnected(SOCKET s, Addr addr)
{
	cout << "closed client " << addr.toString() << ", fd: " << s << endl;
}

void IocpServer::notifyPackageReceived(ClientContext* pConnClient)
{
	echo(pConnClient);
}

void IocpServer::notifyWritePackage()
{
}

void IocpServer::notifyWriteCompleted()
{
}

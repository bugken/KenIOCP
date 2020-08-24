#ifndef __IOCP_SERVER_H__
#define __IOCP_SERVER_H__
#include "Addr.h"
#include "PerSocketContext.h"
#include <list>
#include <vector>

struct IoContext;
struct ListenContext;
struct AcceptIoContext;
struct ClientContext;

class IocpServer
{
public:
	IocpServer(short listenPort, int maxConnectionCount = 10000);
	IocpServer(const IocpServer&) = delete;
	IocpServer& operator=(const IocpServer&) = delete;
	virtual ~IocpServer();

	bool start();
	bool stop();
	bool shutdown();

	bool send(ClientContext* pConnClient, PBYTE pData, UINT len);

protected:
	//����Ҫstatic _beginthreadex���ܷ���
	static unsigned WINAPI IocpWorkerThread(LPVOID arg);

	HANDLE associateWithCompletionPort(SOCKET s, ULONG_PTR completionKey);
	bool getAcceptExPtr();
	bool getAcceptExSockaddrs();
	bool setKeepAlive(ClientContext* pConnClient, LPOVERLAPPED lpOverlapped,
		int time = 1, int interval = 1);

	bool createListenClient(short listenPort);
	bool createIocpWorker();
	bool exitIocpWorker();
	bool initAcceptIoContext();

	bool postAccept(AcceptIoContext* pIoCtx);
	PostResult postRecv(ClientContext* pConnClient);
	PostResult postSend(ClientContext* pConnClient);

	bool handleAccept(LPOVERLAPPED lpOverlapped, DWORD dwBytesTransferred);
	bool handleRecv(ULONG_PTR lpCompletionKey, LPOVERLAPPED lpOverlapped,
		DWORD dwBytesTransferred);
	bool handleSend(ULONG_PTR lpCompletionKey, LPOVERLAPPED lpOverlapped,
		DWORD dwBytesTransferred);
	bool handleClose(ULONG_PTR lpCompletionKey);

	// Used to avoid access violation.
	void enterIoLoop(ClientContext* pClientCtx);
	int exitIoLoop(ClientContext* pClientCtx);

	void CloseClient(ClientContext* pConnClient);

	//���������ӿͻ��������̰߳�ȫ
	void addClient(ClientContext* pConnClient);
	void removeClient(ClientContext* pConnClient);
	void removeAllClients();

	ClientContext* allocateClientContext(SOCKET s);
	void releaseClientContext(ClientContext* pConnClient);

	void echo(ClientContext* pConnClient);

	//�ص�����
	virtual void notifyNewConnection(ClientContext* pConnClient);
	//virtual void notifyDisconnected(ClientContext* pConnClient);
	virtual void notifyDisconnected(SOCKET s, Addr addr);
	virtual void notifyPackageReceived(ClientContext* pConnClient);
	virtual void notifyWritePackage();
	virtual void notifyWriteCompleted();

private:
	bool m_bIsShutdown; //�ر�ʱ���˳������߳�

	short m_listenPort;
	HANDLE m_hComPort; //��ɶ˿�
	HANDLE m_hExitEvent; //�˳��߳��¼�
	//postSend��Ӧ��д��������ɣ����Խ�����һ��Ͷ��
	HANDLE m_hWriteCompletedEvent; 

	void* m_lpfnAcceptEx; //acceptEx����ָ��
	void* m_lpfnGetAcceptExAddr; //GetAcceptExSockaddrs����ָ��

	int m_nWorkerCnt; //io�����߳�����
	DWORD m_nConnClientCnt; //�����ӿͻ�������
	DWORD m_nMaxConnClientCnt; //���ͻ�������

	ListenContext* m_pListenCtx; //����������

	std::list<ClientContext*> m_connectedClientList; //�����ӿͻ�������
	std::list<ClientContext*> m_freeClientList; //���е�ClientContext����
	CRITICAL_SECTION m_csClientList; //�����ͻ�������std::list<ClientContext*>

	std::vector<HANDLE> m_hWorkerThreads; //�����߳̾���б�
	std::vector<AcceptIoContext*> m_acceptIoCtxList; //�������ӵ�IO�������б�
};

#endif // !__IOCP_SERVER_H__
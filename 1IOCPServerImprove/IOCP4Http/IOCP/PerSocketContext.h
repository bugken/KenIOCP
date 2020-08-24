#ifndef __CLIENT_CONTEXT_H__
#define __CLIENT_CONTEXT_H__
#include "Addr.h"
#include "Buffer.h"
#include "PerIoContext.h"
#include <algorithm>
#include <string>
#include <queue>
#include <map>

struct ListenContext
{
	ListenContext(short port, const std::string& ip = "0.0.0.0");

	SOCKET m_socket; //����socket
	SOCKADDR_IN m_addr; //������ַ
};

struct IoContext;
struct RecvIoContext;

struct ClientContext
{
	ClientContext(const SOCKET& socket = INVALID_SOCKET);
	//socket��IocpServer�ͷ�
	~ClientContext();
	void reset();

	void appendToBuffer(PBYTE pInBuf, size_t len);
	void appendToBuffer(const std::string& inBuf);

	SOCKET m_socket; //�ͻ���socket
	Addr m_addr; //�ͻ��˵�ַ
	//Avoids Access Violation����ֵΪ0ʱ�����ͷ�ClientContext
	ULONG m_nPendingIoCnt; 
	RecvIoContext* m_recvIoCtx;
	IoContext* m_sendIoCtx;
	Buffer m_inBuf;
	Buffer m_outBuf;
	std::queue<Buffer> m_outBufQueue;
	CRITICAL_SECTION m_csLock; //����ClientContext

	DWORD m_nLastHeartbeatTime;
};

#endif // !__CLIENT_CONTEXT_H__


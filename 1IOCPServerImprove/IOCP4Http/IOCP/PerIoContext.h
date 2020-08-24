#ifndef __IO_CONTEXT_H__
#define __IO_CONTEXT_H__
#include <WinSock2.h>
#include <Windows.h>

//���ó�4K�ı���
#define MAX_BUFFER_LEN 1024 * 8

#define ACCEPT_ADDRS_SIZE (sizeof(SOCKADDR_IN) + 16)
#define DOUBLE_ACCEPT_ADDRS_SIZE  ((sizeof(SOCKADDR_IN) + 16) * 2)

enum PostType
{
	UNKNOWN, // ���ڳ�ʼ����������
	ACCEPT, // ��־Ͷ�ݵ�Accept����
	SEND, // ��־Ͷ�ݵ��Ƿ��Ͳ���
	RECV, // ��־Ͷ�ݵ��ǽ��ղ���
};

enum PostResult
{
	PostResultSucc, //�ɹ�
	PostResultFailed, //ʧ��
	PostResultInvalidSocket, //�׽�����Ч
};

//Ϊ�˱�֤���½ṹ���һ��������Ա��OVERLAPPED��
//����ʹ���麯���������麯�������һ�����������ָ�룩
struct IoContext
{
	//ÿһ���ص�io������Ҫ��һ��OVERLAPPED�ṹ
	OVERLAPPED m_Overlapped;
	PostType m_PostType;
	WSABUF m_wsaBuf;

	IoContext(PostType type);
	~IoContext();
	void resetBuffer();
};

struct AcceptIoContext : public IoContext
{
	AcceptIoContext(SOCKET acceptSocket = INVALID_SOCKET);
	~AcceptIoContext();
	void resetBuffer();
	SOCKET m_acceptSocket; //�������ӵ�socket
	BYTE m_accpetBuf[MAX_BUFFER_LEN]; //����acceptEx��������
};

struct RecvIoContext : public IoContext
{
	RecvIoContext();
	~RecvIoContext();
	void resetBuffer();
	BYTE m_recvBuf[MAX_BUFFER_LEN];
};

#endif // !__IO_CONTEXT_H__

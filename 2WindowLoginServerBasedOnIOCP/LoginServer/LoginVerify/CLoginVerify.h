/*
**@autoer libing
**@describe
	һ���ͻ���ֻ��Ψһһ��CLoginVerify���󣬶�Ӧ��һ����¼��ɫ�����к�����������json��ʽ���ַ�����������չ��
	���ں���д��virtual����һ��Ҫ���η�װ�أ���_����
**@useway
	1��InitEnv��ʼ��socket����
	2��Login��¼����¼�ɹ������GetDeadLine��ȡ�û�ʹ�����ޣ�GetUserLevel��ȡ�û��ȼ���ҵ����
	3����¼�ɹ��󣬵���KeepAlive�����ظ���¼У���̣߳�ͬʱ������������
   	  �ڿͻ��˳����п���һ����ʱ���߳���У��CLoginVerify��clientState_������У��
    ����һ�����Ĳ������������Ȥ����Ը�Ϊ�ص����߽ӿڵĻ��ƣ���������϶�һ���͸��
*/

#pragma once
#include <iostream>
#include <WinSock2.h>

#include "../Include/rapidjson/rapidjson.h"
#include "../Include/rapidjson/document.h"
#include "../Include/rapidjson/prettywriter.h"  
#include "../Include/rapidjson/stringbuffer.h" 
using namespace rapidjson;

//�û�����
enum USER_TYPE {
    NON,
    STOCK,
    FUTURES
};

//�û�����
enum USER_LEVEL{
    NONE,
    PRIMARY,
    INTERMEDIATE,
    SENIOR
};

enum CLIENT_STATE
{
	CLIENT_INITIAL = -1,	//��ʼ״̬
	CLIENT_CONNECTED = 0,		//������
	CLIENT_RELOGIN = 1,		//�ظ���¼
	CLIENT_DISCONNECT = 2	//����
};

class CLoginVerify
{
public:
    CLoginVerify();

    // ��ʼ��env
    int InitEnv(const char* pServerInfo);

    // �û���½
    virtual int Login(const char* pUserInfo);

    // �û�ע��
    virtual int Register(const char* pUserInfo);

    // ��ȡ���°汾�ͻ�����Ϣ
    virtual const std::string GetLatestVersion();

    // ��ȡϵͳʱ��
    virtual const std::string GetSystemTime();

    // ��ȡ�û���ֹ����
    virtual const std::string GetDeadLine();

    // �ж��û��Ƿ�ʧЧ
    virtual bool BeyondDeadLine();

    // ��ȡ�û�����
    virtual USER_TYPE GetUserType();

    // ��ȡ�û��ȼ�
    virtual USER_LEVEL GetUserLevel();

    // �����û���ֹ����
    virtual int SetDeadLine(const char* pDeadLine);

	//Reset password
	virtual int ResetPassWord(const char* pUserInfo);

    // ����env
    void cleanEnv();

    // ��������
    void KeepAlive();

    // Get socket
    SOCKET GetSocket();

    //Set Socket
    void SetSocket(SOCKET sock);

private:
    void CreateLog();

private:
    std::string userName_;

    SOCKET clientSocket_;

public:
	int clientState_;		//�ͻ�������״̬
};
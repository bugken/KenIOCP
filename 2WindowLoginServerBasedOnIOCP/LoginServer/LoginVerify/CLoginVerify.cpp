#include "CLoginVerify.h"
#include "logger.h"
#include "cs_const.h"
#include "md5.h"
using namespace std;
#pragma comment(lib, "ws2_32.lib")

CLoginVerify::CLoginVerify()
{
    clientState_ = CLIENT_INITIAL;

	CreateLog();
}

void CLoginVerify::CreateLog()
{
    struct tm *newtime;
    char tmpbuf[128];
    time_t lt1;
    time(&lt1);
    newtime=localtime(&lt1);
    CreateDirectory(TEXT(".\\log_c"),NULL);
    strftime(tmpbuf, 128, "%Y_%m_%d_log.txt", newtime);
    string filename(tmpbuf);
    //string logfile(".login_log");
    string full_logname(".\\log_c\\");
    //string current_logFile = //getenv("HOME") + full_logname + filename + logfile;
    //string full_logname = filename
    //string current_logFile(".\\log.txt");
    string current_logFile(full_logname + filename);
    INIT_LOGGER(current_logFile.c_str());
    INFO_LOGGER << "INFO_LOGGER process param!--------------------------------------" << END_LOGGER;
    INFO_LOGGER << "LOGGER: creat logFile at " << " " << current_logFile << END_LOGGER;
}

// ��ʼ��env
int CLoginVerify::InitEnv(const char* pServerInfo)
{
	Document document;
	document.Parse<0>((char*)pServerInfo, strlen(pServerInfo));
	if (document.HasParseError())
	{
		ERROR_LOGGER << "Parse ServerInfo Error, ServerInfo:" << pServerInfo << ", Error: " << document.GetParseError() << END_LOGGER;
		return -1;
	}

	if (!document.HasMember("port") || !document["port"].IsInt())
	{
		ERROR_LOGGER << "Parse port Error, ServerInfo:" << pServerInfo << END_LOGGER;
		return -1;
	}

	if (!document.HasMember("serverIp") || !document["serverIp"].IsString())
	{
		ERROR_LOGGER << "Parse serverIp Error, ServerInfo:" << pServerInfo << END_LOGGER;
		return -1;
	}

    //�����׽���  
    WSADATA wsaData;
    int errorCode = 0;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        errorCode = WSAGetLastError();
        ERROR_LOGGER << "LOGGER: Failed to load Winsock: " << errorCode << END_LOGGER;
        return errorCode;
    }

    SOCKADDR_IN addrSrv;
    addrSrv.sin_family = AF_INET;
    int port = document["port"].GetInt();
    addrSrv.sin_port = htons(port);
    string serverIp = document["serverIp"].GetString();
    addrSrv.sin_addr.S_un.S_addr = inet_addr(serverIp.c_str());

    //�����׽���  
    clientSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (SOCKET_ERROR == clientSocket_)
    {
        errorCode = WSAGetLastError();
        ERROR_LOGGER << "Create Socket failed: " << errorCode << END_LOGGER;
        return errorCode;
    }

    //�������������������  
    if (connect(clientSocket_, (struct  sockaddr*)&addrSrv, sizeof(addrSrv)) == INVALID_SOCKET)
	{
        errorCode = WSAGetLastError();
        ERROR_LOGGER << "Connect failed: " << errorCode << END_LOGGER;
        return errorCode;
    }

	clientState_ = CLIENT_CONNECTED;
    return 0;
}

int CLoginVerify::Login(const char* pUserInfo)
{
	Document document;
	document.Parse<0>((char*)pUserInfo, strlen(pUserInfo));
	if (document.HasParseError())
	{
		ERROR_LOGGER << "Parse ServerInfo Error, ServerInfo:" << pUserInfo << ", Error: " << document.GetParseError() << END_LOGGER;
		return -1;
	}

	if (!document.HasMember("userName") || !document["userName"].IsString())
	{
		ERROR_LOGGER << "Parse userName Error, ServerInfo:" << pUserInfo << END_LOGGER;
		return -1;
	}

	if (!document.HasMember("passWord") || !document["passWord"].IsString())
	{
		ERROR_LOGGER << "Parse passWord Error, ServerInfo:" << pUserInfo << END_LOGGER;
		return -1;
	}

	//�����û�
	userName_ = document["userName"].GetString();

	//��������
	document["type"] = CS_LOGIN;

	//��������
	std::string strPwd = document["passWord"].GetString();
    Md5Encode encodeObj;
    std::string encodePassWord = encodeObj.Encode(strPwd);
	Value& sPwd = document["passWord"];
	sPwd.SetString(encodePassWord.c_str(), encodePassWord.length());

	//ת���ַ���
	StringBuffer buffer;
	Writer<StringBuffer> writer(buffer);
	document.Accept(writer);
	string sendMes = buffer.GetString();

	//���ó�ʱ
	int timeOver = 5000;
	::setsockopt(clientSocket_, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeOver, sizeof(timeOver));

    int nRet = send(clientSocket_, sendMes.c_str(), sendMes.length(), 0);
    if (nRet != sendMes.length()) 
    {
        int nErrCode = WSAGetLastError();
        ERROR_LOGGER << "Login send failed: " << nErrCode << END_LOGGER;
        return nErrCode;
    }

    char buff[1024] = { 0 };
    int ret = recv(clientSocket_, buff, 1024,0);
    if (ret < 0) 
    {
        int nErrCode = WSAGetLastError();
        ERROR_LOGGER << "Login Recv failed: " << WSAGetLastError() << END_LOGGER;
        return nErrCode;
    }
    int returnCode = atoi(buff);
    INFO_LOGGER << "Login return code: " << returnCode << END_LOGGER;
    return returnCode;
}

// �û�ע��
int CLoginVerify::Register(const char* pUserInfo)
{
	Document document;
	document.Parse<0>((char*)pUserInfo, strlen(pUserInfo));
	if (document.HasParseError())
	{
		ERROR_LOGGER << "Parse ServerInfo Error, ServerInfo:" << pUserInfo << ", Error: " << document.GetParseError() << END_LOGGER;
		return -1;
	}

	if (!document.HasMember("userName") || !document["userName"].IsString())
	{
		ERROR_LOGGER << "Parse userName Error, ServerInfo:" << pUserInfo << END_LOGGER;
		return -1;
	}

	if (!document.HasMember("passWord") || !document["passWord"].IsString())
	{
		ERROR_LOGGER << "Parse passWord Error, ServerInfo:" << pUserInfo << END_LOGGER;
		return -1;
	}

	//��������
	document["type"] = CS_REGISTER;

	//��������
	std::string strPwd = document["passWord"].GetString();
	Md5Encode encodeObj;
	std::string encodePassWord = encodeObj.Encode(strPwd);
	Value &sPwd = document["passWord"];
	sPwd.SetString(encodePassWord.c_str(), encodePassWord.length());

	//ת���ַ���
	StringBuffer buffer;
	Writer<StringBuffer> writer(buffer);
	document.Accept(writer);
	string sendMes = buffer.GetString();

    int nRet = send(clientSocket_, sendMes.c_str(), sendMes.length(), 0);
    if (nRet != sendMes.length()) 
    {
        int nErrCode = WSAGetLastError();
        ERROR_LOGGER << "Register send failed: " << nErrCode << END_LOGGER;
        return nErrCode;
    }

    char buff[1024] = { 0 };
    int ret = recv(clientSocket_, buff, 1024, 0);
    if (ret < 0)
    {
        int nErrCode = WSAGetLastError();
        ERROR_LOGGER << "Register Recv failed: " << nErrCode << END_LOGGER;
        return nErrCode;
    }
	{
		int returnCode = atoi(buff);
		INFO_LOGGER << "return code: " << returnCode << END_LOGGER;
		return returnCode;
	}
}

// ��ȡ���°汾�ͻ�����Ϣ
const std::string CLoginVerify::GetLatestVersion()
{
	GenericStringBuffer<ASCII<> > JsonStringBuff;
	Writer<GenericStringBuffer<ASCII<> >  > JsonWriter(JsonStringBuff);
	JsonWriter.StartObject();
	JsonWriter.Key("type");
	JsonWriter.Int(CS_GETLASTESTVER);
	JsonWriter.EndObject();
	string sendMes = JsonStringBuff.GetString();

    int nRet = send(clientSocket_, sendMes.c_str(), sendMes.length(), 0);
    if (nRet != sendMes.length()) {
        ERROR_LOGGER << "GetLatestVersion send failed: " << WSAGetLastError() << END_LOGGER;
        return NULL;
    }

    char buff[1024] = { 0 };
    int ret = recv(clientSocket_, buff, 1024, 0);
    if (ret < 0) 
    {
        ERROR_LOGGER << "GetLatestVersion recv failed: " << WSAGetLastError() << END_LOGGER;
    }
    INFO_LOGGER <<"��ѯ�����°汾: " << buff << END_LOGGER;
    return buff;
}

// ��ȡ�û���ֹ����
const std::string CLoginVerify::GetDeadLine()
{
	GenericStringBuffer<ASCII<> > JsonStringBuff;
	Writer<GenericStringBuffer<ASCII<> >  > JsonWriter(JsonStringBuff);
	JsonWriter.StartObject();
	JsonWriter.Key("type");
	JsonWriter.Int(CS_GETDEADLINE);
	JsonWriter.EndObject();
	string sendMes = JsonStringBuff.GetString();

    int nRet = send(clientSocket_, sendMes.c_str(), sendMes.length(), 0);
    if (nRet != sendMes.length()) {
        ERROR_LOGGER << "GetDeadLine send failed: " << WSAGetLastError() << END_LOGGER;
        return NULL;
    }

    char buff[1024] = { 0 };
    int ret = recv(clientSocket_, buff, 1024, 0);
    if (ret < 0) 
    {
        ERROR_LOGGER << "GetDeadLine recv failed: " << WSAGetLastError() << END_LOGGER;
    }
    INFO_LOGGER <<"��ѯ����ֹ����: " << buff << END_LOGGER;
    return buff;
}

const std::string CLoginVerify::GetSystemTime()
{
	GenericStringBuffer<ASCII<> > JsonStringBuff;
	Writer<GenericStringBuffer<ASCII<> >  > JsonWriter(JsonStringBuff);
	JsonWriter.StartObject();
	JsonWriter.Key("type");
	JsonWriter.Int(CS_GETSYSTEMTIME);
	JsonWriter.EndObject();
	string sendMes = JsonStringBuff.GetString();

    int nRet = send(clientSocket_, sendMes.c_str(), sendMes.length(), 0);
    if (nRet != sendMes.length()) 
    {
        ERROR_LOGGER << "GetSystemTime send failed: " << WSAGetLastError() << END_LOGGER;
        return NULL;
    }

    char buff[1024] = { 0 };
    int ret = recv(clientSocket_, buff, 1024, 0);
    if (ret < 0) 
    {
        ERROR_LOGGER << "GetSystemTime recv failed: " << WSAGetLastError() << END_LOGGER;
    }
    INFO_LOGGER <<"��ѯ��ϵͳ����: " << buff << END_LOGGER;
    return buff;
}

bool CLoginVerify::BeyondDeadLine()
{
	GenericStringBuffer<ASCII<> > JsonStringBuff;
	Writer<GenericStringBuffer<ASCII<> >  > JsonWriter(JsonStringBuff);
	JsonWriter.StartObject();
	JsonWriter.Key("type");
	JsonWriter.Int(CS_BEYONDDEADLINE);
	JsonWriter.EndObject();
	string sendMes = JsonStringBuff.GetString();

    int nRet = send(clientSocket_, sendMes.c_str(), sendMes.length(), 0);
    if (nRet != sendMes.length()) 
    {
        ERROR_LOGGER << "BeyondDeadLine send failed: " << WSAGetLastError() << END_LOGGER;
        return NULL;
    }

    char buff[1024] = { 0 };
    int ret = recv(clientSocket_, buff, 1024, 0);
    if (ret < 0) 
    {
        ERROR_LOGGER << "BeyondDeadLine recv failed: " << WSAGetLastError() << END_LOGGER;
    }
    INFO_LOGGER <<"�Ƿ񳬳���ֹ����: " << buff << END_LOGGER;
    int nResult =  atoi(buff);
    if (nResult == 0)
    {
        return false;
    }
    return true;
}

// ��ȡ�û�����
USER_TYPE CLoginVerify::GetUserType()
{
	GenericStringBuffer<ASCII<> > JsonStringBuff;
	Writer<GenericStringBuffer<ASCII<> >  > JsonWriter(JsonStringBuff);
	JsonWriter.StartObject();
	JsonWriter.Key("type");
	JsonWriter.Int(CS_GETUSERTYPE);
	JsonWriter.EndObject();
	string sendMes = JsonStringBuff.GetString();

    int nRet = send(clientSocket_, sendMes.c_str(), sendMes.length(), 0);
    if (nRet != sendMes.length()) 
    {
        ERROR_LOGGER << "GetUserType send failed: " << WSAGetLastError() << END_LOGGER;
        return USER_TYPE::NON;
    }

    char buff[1024] = { 0 };
    int ret = recv(clientSocket_, buff, 1024, 0);
    if (ret < 0) 
    {
        ERROR_LOGGER << "GetUserType recv failed: " << WSAGetLastError() << END_LOGGER;
    }
    int nResult = atoi(buff);
    if (nResult == 1)
    {
        INFO_LOGGER << userName_ << " �û�����: ��Ʊ" << END_LOGGER;
        return USER_TYPE::STOCK;
    }
    else if(nResult == 2)
    {
        INFO_LOGGER << userName_ << " �û�����: �ڻ�" << END_LOGGER;
        return USER_TYPE::FUTURES;
    }
    return USER_TYPE::NON;
}

// ��ȡ�û��ȼ�
USER_LEVEL CLoginVerify::GetUserLevel()
{
	GenericStringBuffer<ASCII<> > JsonStringBuff;
	Writer<GenericStringBuffer<ASCII<> >  > JsonWriter(JsonStringBuff);
	JsonWriter.StartObject();
	JsonWriter.Key("type");
	JsonWriter.Int(CS_GETUSERLEVEL);
	JsonWriter.EndObject();
	string sendMes = JsonStringBuff.GetString();

    int nRet = send(clientSocket_, sendMes.c_str(), sendMes.length(), 0);
    if (nRet != sendMes.length()) 
    {
        ERROR_LOGGER << "GetUserLevel send failed: " << WSAGetLastError() << END_LOGGER;
        return USER_LEVEL::NONE;
    }

    char buff[1024] = { 0 };
    int ret = recv(clientSocket_, buff, 1024, 0);
    if (ret < 0) 
    {
        ERROR_LOGGER << "GetUserLevel recv failed: " << WSAGetLastError() << END_LOGGER;
    }
    int nResult =  atoi(buff);
    INFO_LOGGER << userName_ << " �û��ȼ�: " << buff << END_LOGGER;
    if (nResult == 1)
    {
        return USER_LEVEL::PRIMARY;
    }
    else if(nResult == 2)
    {
        return USER_LEVEL::INTERMEDIATE;
    }
    else if (nResult == 3)
    {
        return USER_LEVEL::SENIOR;
    }
    return USER_LEVEL::NONE;
}

// �����û���ֹ����
int CLoginVerify::SetDeadLine(const char* pDeadLine)
{
    return 0;
}

// ����env
void CLoginVerify::cleanEnv()
{
    if (clientSocket_ != NULL)
    {
        closesocket(clientSocket_);
        WSACleanup();
    }
    return;
}

SOCKET CLoginVerify::GetSocket()
{
    return clientSocket_;
}

void CLoginVerify::SetSocket(SOCKET sock)
{
    clientSocket_ = sock;
}

DWORD WINAPI CreateKeepThread(LPVOID lpParameter)
{
	//��CreateReLoginThread�߳��У�������У���ظ���¼��ͬʱ����������������߳��ò���
	return 0;

    CLoginVerify* tmpParam = (CLoginVerify*)lpParameter;
    BOOL bNoDelay = TRUE;
    ::setsockopt(tmpParam->GetSocket(), IPPROTO_TCP, TCP_NODELAY , (char FAR *)&bNoDelay, sizeof(BOOL));

	GenericStringBuffer<ASCII<> > JsonStringBuff;
	Writer<GenericStringBuffer<ASCII<> >  > JsonWriter(JsonStringBuff);
	JsonWriter.StartObject();
	JsonWriter.Key("type");
	JsonWriter.Int(CS_KEEPALIVE);
	JsonWriter.EndObject();
	string sendMes = JsonStringBuff.GetString();

    while(1)
    {
        Sleep(10000);
        if (tmpParam->GetSocket() == NULL)
        {
            return 0;
        }
        if (tmpParam->clientState_ != CLIENT_CONNECTED)
        {
            return 0;
        }

        int nRet = send(tmpParam->GetSocket(), sendMes.c_str(), sendMes.length(), 0);
        if (nRet < 0)
        {
            int errCode = WSAGetLastError();
            INFO_LOGGER << "�����쳣: " << errCode << END_LOGGER;
            tmpParam->clientState_ = CLIENT_DISCONNECT;
            closesocket(tmpParam->GetSocket());
            tmpParam->SetSocket(NULL);
            return 0;
        }
    }
}

DWORD WINAPI CreateReLoginThread(LPVOID lpParameter)
{
    CLoginVerify* tmpParam = (CLoginVerify*)lpParameter;
    BOOL bNoDelay = TRUE;
    ::setsockopt(tmpParam->GetSocket(), IPPROTO_TCP, TCP_NODELAY , (char FAR *)&bNoDelay, sizeof(BOOL));

	GenericStringBuffer<ASCII<> > JsonStringBuff;
	Writer<GenericStringBuffer<ASCII<> >  > JsonWriter(JsonStringBuff);
	JsonWriter.StartObject();
	JsonWriter.Key("type");
	JsonWriter.Int(CS_RELOGINTHREAD);
	JsonWriter.EndObject();
	string sendMes = JsonStringBuff.GetString();

    while(1)
    {
        Sleep(5000);
        if (tmpParam->GetSocket() == NULL)
        {
            return 0;
        }
        if (tmpParam->clientState_ != CLIENT_CONNECTED)
        {
            return 0;
        }
        int nRet = send(tmpParam->GetSocket(), sendMes.c_str(), sendMes.length(), 0);
        if (SOCKET_ERROR == nRet)
        {
            tmpParam->clientState_ = CLIENT_CONNECTED;
            closesocket(tmpParam->GetSocket());
            tmpParam->SetSocket(NULL);
            INFO_LOGGER << "1 ��⵽�����쳣����ǰ�û��˳�!" << END_LOGGER;
            cout << "1 ��⵽�����쳣����ǰ�û��˳�!" << endl;
            return 0;
        }

        char buff[1024] = { 0 };
        struct timeval tv_out;
        tv_out.tv_sec = 8;
        tv_out.tv_usec = 0;
        int timeOver = 1000;
        ::setsockopt(tmpParam->GetSocket(), SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeOver, sizeof(timeOver));
        nRet = recv(tmpParam->GetSocket(), buff, 1024, 0);
        if (SOCKET_ERROR == nRet)
        {
			tmpParam->clientState_ = CLIENT_CONNECTED;
			closesocket(tmpParam->GetSocket());
			tmpParam->SetSocket(NULL);
			INFO_LOGGER << "1 ��⵽�����쳣����ǰ�û��˳�!" << END_LOGGER;
			cout << "1 ��⵽�����쳣����ǰ�û��˳�!" << endl;
			return 0;
        }

        int iState = atoi(buff);
        cout << "�յ�server info: "<< nRet << endl;
        if (1 == iState)
        {
			tmpParam->clientState_ = CLIENT_RELOGIN;
            closesocket(tmpParam->GetSocket());
            tmpParam->SetSocket(NULL);
            INFO_LOGGER << "��⵽�û��ظ���½����ǰ�û��˳�!" << END_LOGGER;
            cout << "��⵽�û��ظ���½����ǰ�û��˳�!" << endl;
            return 0;
        }
    }
    return 0;
}

void CLoginVerify::KeepAlive()
{
    HANDLE hThread = ::CreateThread(NULL, 0,  CreateReLoginThread, (LPVOID)this, 0, NULL);
    HANDLE hThread2 = ::CreateThread(NULL, 0, CreateKeepThread, (LPVOID)this, 0, NULL);
    if (hThread == NULL)
    {
        ERROR_LOGGER << "Failed to create a new thread! Error code: " << WSAGetLastError() << END_LOGGER;
        ::WSACleanup();
    }
    ::CloseHandle(hThread);
    ::CloseHandle(hThread2);
}

int CLoginVerify::ResetPassWord(const char* pUserInfo)
{
	Document document;
	document.Parse<0>((char*)pUserInfo, strlen(pUserInfo));
	if (document.HasParseError())
	{
		ERROR_LOGGER << "Parse ServerInfo Error, ServerInfo:" << pUserInfo << ", Error: " << document.GetParseError() << END_LOGGER;
		return -1;
	}

	if (!document.HasMember("oldPassWord") || !document["oldPassWord"].IsString())
	{
		ERROR_LOGGER << "Parse oldPassWord Error, ServerInfo:" << pUserInfo << END_LOGGER;
		return -1;
	}

	if (!document.HasMember("passWord") || !document["passWord"].IsString())
	{
		ERROR_LOGGER << "Parse passWord Error, ServerInfo:" << pUserInfo << END_LOGGER;
		return -1;
	}

	if (!document.HasMember("newPassWord") || !document["newPassWord"].IsString())
	{
		ERROR_LOGGER << "Parse newPassWord Error, ServerInfo:" << pUserInfo << END_LOGGER;
		return -1;
	}

	//��������
	document["type"] = CS_RESETPASSWORD;

	//���ܾ�����
    string oldPassWord = document["oldPassWord"].GetString();
	Md5Encode encodeObj;
    std::string encodePassWord = encodeObj.Encode(oldPassWord);
	Value &sOldPwd = document["oldPassWord"];
	sOldPwd.SetString(encodePassWord.c_str(), encodePassWord.length());

	//���ܾ�����
    string newPassWord = document["newPassWord"].GetString();
    encodePassWord = encodeObj.Encode(newPassWord);
	Value &sNewPwd = document["newPassWord"];
	sNewPwd.SetString(encodePassWord.c_str(), encodePassWord.length());

	//ת���ַ���
	StringBuffer buffer;
	Writer<StringBuffer> writer(buffer);
	document.Accept(writer);
	string sendMes = buffer.GetString();

    int nRet = send(clientSocket_, sendMes.c_str(), sendMes.length(), 0);
    if (nRet != sendMes.length()) 
    {
        int nErrCode = WSAGetLastError();
        ERROR_LOGGER << "Reset PassWord send failed: " << nErrCode << END_LOGGER;
        return nErrCode;
    }

    char buff[1024] = { 0 };
    int ret = recv(clientSocket_, buff, 1024, 0);
    if (ret < 0)
    {
        int nErrCode = WSAGetLastError();
        ERROR_LOGGER << "Reset PassWord Recv failed: " << nErrCode << END_LOGGER;
        return nErrCode;
    }
    else
    {
        int returnCode = atoi(buff);
        INFO_LOGGER << "Reset PassWord return code: " << returnCode << END_LOGGER;
        return returnCode;
    }
}
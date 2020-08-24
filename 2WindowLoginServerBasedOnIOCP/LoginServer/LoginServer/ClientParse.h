#pragma once
#include <string>
#include <map>

#include "mysql/include/mysql.h"

#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"  
#include "rapidjson/stringbuffer.h" 
using namespace rapidjson;

#include "object_pool.h"
#include "logger.h"

#define DB_IP		"47.102.96.37"//"localhost"//
#define DB_USER		"lblogin"
#define DB_PWD		"575619"
#define DB_NAME		"stockalert"
#define TABLE_NAME	"user"


//���ݿ�������
typedef struct SqlConNode
{
	MYSQL *pSqlCon;	//���ݿ����Ӷ���
	SqlConNode* pNext;	//��һ���ڵ�

	SqlConNode()
	{
		pSqlCon = NULL;
		pNext = NULL;

		Init();
	}
	~SqlConNode()
	{
		Unit();
	}

	void Init()
	{
		pSqlCon = mysql_init((MYSQL*)0);
		if (pSqlCon)
		{
			char value = 1;
			mysql_options(pSqlCon, MYSQL_OPT_RECONNECT, &value);
			if (mysql_real_connect(pSqlCon, DB_IP, DB_USER, DB_PWD, DB_NAME, 3306, NULL, 0))
			{
				if (!mysql_select_db(pSqlCon, DB_NAME))
				{
					mysql_query(pSqlCon, "SET NAMES UTF8");
					INFO_LOGGER << "Select successfully the database!" << END_LOGGER;
				}
			}
			else
			{
				std::string strErr = mysql_error(pSqlCon);
				WARNNING_LOGGER << "Connect Database Error " << strErr << END_LOGGER;
				pSqlCon = NULL;
			}
		}
		else
		{
			std::string strErr = mysql_error(pSqlCon);
			WARNNING_LOGGER << "Connect Database Error " << strErr << END_LOGGER;
			pSqlCon = NULL;
		}
	}

	void Unit()
	{
		if (pSqlCon)
		{
			mysql_close(pSqlCon);
		}
	}
};

//�ͻ���ҵ���࣬һ�����Ӷ�Ӧһ���࣬����ֻ����ҵ�񣬲���������ͨ��
class CClientParse
{
public:
	CClientParse();
	~CClientParse();

public:
	//�����ͻ��˴������ַ�����Ϣ
	void Run(const std::string& strMsg);

	//��ȡҵ����Ĵ�����
	void GetReturnValue(char *pBuf, int nSize, unsigned long &nLength);

	//��ȡ�û���
	inline std::string GetUserName() const {
		return m_userName;
	}

private:
	//ƴ�Ӵ�����Ϣ���ڷ���
	void ErrorMsg(const int nErrno = -1);

	//��ȡĬ�ϵ�ʹ������
	std::string GetDefaultExp();

	//У���Ƿ��ظ���¼
	void UpdateLoginState(const std::string& strUser);

	//���ĳ���û��ĵ�½̬
	void RemoveLoginState(const std::string& strUser);

private:
	//��¼
	void Login(Document& doc);

	//ע��
	void Register(Document& doc);

	//��������
	void ResetPassWord(Document& doc);

	//��ȡ�û�����
	void GetUserLevel(Document& doc);	

	//��ȡ�û���������Ȩ��
	void GetFunctionPermission(Document& doc);

	//��ȡʹ��ʱ��
	void BeyondDeadLine(Document& doc);

	//�ظ���¼��У��
	void CheckRelogin(Document& doc);

private:
	std::string m_strResult;	//�˴��򵥴���ֱ�����ַ����ˣ������Լ�ƴ��json��

	std::string m_userName;		//�û���

	std::string m_userId;		//�û�Id

	static CLock s_clientLock;	//��

	static CObjectPool<SqlConNode> s_sqlPool;	//���ݿ��

public:
	bool m_bReLogin = false;	//�Ƿ����ظ���¼

	static std::map<std::string, CClientParse*> s_ClientMap;	//�����û�����client��Ϣ�����ڴ�����

	static void DeleteSelfLoginState(CClientParse* pClient);	//�����ǰ�ĵ�¼״̬
};


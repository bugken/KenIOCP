#include "pch.h"
#include "ClientParse.h"


std::string string2UTF8(const std::string & str) {
	int nwLen = ::MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, NULL, 0);

	wchar_t * pwBuf = new wchar_t[nwLen + 1];//һ��Ҫ��1����Ȼ�����β��  
	ZeroMemory(pwBuf, nwLen * 2 + 2);

	::MultiByteToWideChar(CP_ACP, 0, str.c_str(), str.length(), pwBuf, nwLen);

	int nLen = ::WideCharToMultiByte(CP_UTF8, 0, pwBuf, -1, NULL, NULL, NULL, NULL);

	char * pBuf = new char[nLen + 1];
	ZeroMemory(pBuf, nLen + 1);

	::WideCharToMultiByte(CP_UTF8, 0, pwBuf, nwLen, pBuf, nLen, NULL, NULL);

	std::string retStr(pBuf);

	delete[]pwBuf;
	delete[]pBuf;

	pwBuf = NULL;
	pBuf = NULL;

	return retStr;
}

CClientParse::CClientParse()
{

}

CClientParse::~CClientParse()
{
	
}

void CClientParse::Run(const std::string& strMsg)
{
	int nType = CS_UNDEFINED;

	if (strMsg.empty())
		return;

	//json����
	Document document;
	document.Parse((char*)strMsg.c_str(), strMsg.length());
	if (document.HasParseError())
	{
		ErrorMsg(ERROR_PARSE_JSON);
		//WARNNING_LOGGER << "����json����json��Ϊ;" << strMsg << END_LOGGER;
		return;
	}

	if (!document.IsObject())
	{
		ErrorMsg(ERROR_PARSE_JSON);
		//WARNNING_LOGGER << "�����Ǳ�׼��json��" << END_LOGGER;
		return;
	}

	//��ȡҵ������
	if (document.HasMember("type") && document["type"].IsInt())
	{
		nType = document["type"].GetInt();
	}
	if (nType == CS_UNDEFINED)
	{
		//WARNNING_LOGGER << "ҵ������Ϊ: " << m_nType << "����֧��" << END_LOGGER;
		return;
	}

	//std::cout << m_nType << " begin  :" << this  << " " << m_strResult << std::endl;

	//std::cout << m_nType << std::endl;

	//����ͬ��ҵ��
	switch (nType)
	{
	case CS_LOGIN:
		Login(document);
		break;
	case CS_REGISTER:
		Register(document);
		break;
	case CS_RESETPASSWORD:
		ResetPassWord(document);
		break;
	case CS_BEYONDDEADLINE:
		BeyondDeadLine(document);
		break;
	case CS_GETUSERLEVEL:
		GetUserLevel(document);
		break;
	case CS_GETFUNCTIONPERMISSION:
		GetFunctionPermission(document);
		break;
	case CS_RELOGINTHREAD:
		CheckRelogin(document);
		break;
	case CS_HEARTTHREAD:
		m_strResult = "10000";
		break;
	default:
		break;
	}

	//std::cout << m_nType << " end  :" << this << " " << m_strResult << std::endl;
}

void CClientParse::GetReturnValue(char *pBuf, int nSize, unsigned long &nLength)
{
	if (pBuf == NULL)
		return;

	memset(pBuf, 0, nSize);

	if (m_strResult.length() <= 0)
		return;

	//����������
	if (m_strResult.length() > nSize - 1)
	{
		strcpy_s(pBuf, nSize -1, m_strResult.c_str());
		nLength = nSize - 1;
	}
	else
	{
		strcpy_s(pBuf, m_strResult.length() + 1, m_strResult.c_str());
		nLength = m_strResult.length() + 1;
	}
	 
}

void CClientParse::ErrorMsg(const int nErrno /* = ERROR_UNKNOWN*/)
{
	GenericStringBuffer<ASCII<> > JsonStringBuff;
	Writer<GenericStringBuffer<ASCII<> >  > JsonWriter(JsonStringBuff);

	JsonWriter.StartObject();
	JsonWriter.Key("return_code");
	JsonWriter.Int(nErrno);
	JsonWriter.EndObject();

	m_strResult = JsonStringBuff.GetString();
}

std::string CClientParse::GetDefaultExp()
{
	SYSTEMTIME st;
	GetSystemTime(&st);
	int y = st.wYear + 1;	 //��ȡ�����+1
	int m = st.wMonth;		 //��ȡ��ǰ�·�
	int d = st.wDay;		 //��ü���
	char buf[64] = { 0 };
	sprintf_s(buf, "%d%02d%02d", y, m, d);
	return buf;
}

void CClientParse::UpdateLoginState(const std::string& strUser)
{
	CAutoLock lck(&s_clientLock);

	auto it = s_ClientMap.find(strUser);
	if (it != s_ClientMap.end())
	{
		//�Ѿ���¼��,����value
		it->second->m_bReLogin = true;
		s_ClientMap[strUser] = this;
	}
	else
	{
		//û�е�¼������
		s_ClientMap[strUser] = this;
	}
}

void CClientParse::RemoveLoginState(const std::string& strUser)
{
	CAutoLock lck(&s_clientLock);

	s_ClientMap.erase(strUser);
}

void CClientParse::Login(Document& doc)
{
	if (!doc.HasMember("userName") || !doc["userName"].IsString())
	{
		WARNNING_LOGGER << "�û�����Ϣ����ȷ" << END_LOGGER;
		return;
	}

	if (!doc.HasMember("passWord") || !doc["passWord"].IsString())
	{
		WARNNING_LOGGER << "������Ϣ����ȷ" << END_LOGGER;
		return;
	}

	SqlConNode *pNode = s_sqlPool.GetFree();
	if (!pNode)
	{
		WARNNING_LOGGER << "��ȡ���нڵ�ʧ�ܣ�" << END_LOGGER;
		return;
	}
	MYSQL *pSqlCon = pNode->pSqlCon;
	if (!pSqlCon)
	{
		WARNNING_LOGGER << "��ȡ���ݿ�����ʧ�ܣ�" << END_LOGGER;
		return;
	}

	std::string userName = doc["userName"].GetString();
	std::string sql = "SELECT NAME from user WHERE NAME=\'" + userName + "\'";
	bool existUserName = false;
	int rc = mysql_real_query(pSqlCon, sql.c_str(), sql.length());
	if (rc != 0)
	{
		
		WARNNING_LOGGER << "SELECT NAME from user WHERE NAME=" << userName << " Failed:" << mysql_error(pSqlCon) << END_LOGGER;
		m_strResult = "-3";
		s_sqlPool.Recycle(pNode);
		return;
	}
	MYSQL_RES *res = mysql_store_result(pSqlCon);//�����������res�ṹ����
	if (!mysql_fetch_row(res))
	{
		m_strResult = "-1";
		INFO_LOGGER << "�û�: " << userName << " ������" << END_LOGGER;
		s_sqlPool.Recycle(pNode);
		return;
	}

	std::string passWord = doc["passWord"].GetString();
	sql = "SELECT * from user WHERE NAME=\'" + userName + "\' and PASSWORD=\'" + passWord + "\'";
	rc = mysql_real_query(pSqlCon, sql.c_str(), sql.length());
	if (rc != 0)
	{
		WARNNING_LOGGER << "SELECT * from user WHERE NAME= " << userName << " Failed:" << mysql_error(pSqlCon) << END_LOGGER;
		m_strResult = "-3";
		s_sqlPool.Recycle(pNode);
		return;
	}
	else
	{
		MYSQL_RES *res = mysql_store_result(pSqlCon);//�����������res�ṹ����
		MYSQL_ROW row = NULL;
		if (row = mysql_fetch_row(res))
		{
			if (userName.compare(m_userName) != 0 && !m_userName.empty())
			{
				//���֮ǰ�û��ĵ�¼״̬
				RemoveLoginState(m_userName);
			}
			m_userName = userName;
			//m_strResult = "2000";
			m_strResult = row[0];	//�����û�id
			m_userId = m_strResult;

			UpdateLoginState(m_userName);

			INFO_LOGGER << userName << ":��½�ɹ�: " << END_LOGGER;
		}
		else
		{
			m_strResult = "-2";
			WARNNING_LOGGER << "�˺�:" << userName << " ����:" << passWord << " ����ȷ" << END_LOGGER;
		}
	}

	s_sqlPool.Recycle(pNode);
}

void CClientParse::Register(Document& doc)
{
	std::string userName, passWord, qqCount, expDate, level, userType, referCode;

	//�û����������
	if (doc.HasMember("userName") && doc["userName"].IsString())
	{
		userName = doc["userName"].GetString();
	}
	else
	{
		WARNNING_LOGGER << "�û���������" << END_LOGGER;
		return;
	}

	//����������
	if (doc.HasMember("passWord") && doc["passWord"].IsString())
	{
		passWord = doc["passWord"].GetString();
	}
	else
	{
		WARNNING_LOGGER << "�û���������" << END_LOGGER;
		return;
	}

	//�������Ƽ��벻���ڣ�Ĭ�ϸ�0
	if (doc.HasMember("referCode") && doc["referCode"].IsString())
	{
		referCode = doc["referCode"].GetString();
		if (referCode.empty())
		{
			referCode = "0";
		}
	}
	else
	{
		referCode = "0";
	}

	if (doc.HasMember("qqCount") && doc["qqCount"].IsString())
	{
		qqCount = doc["qqCount"].GetString();
	}

	if (doc.HasMember("exp") && doc["exp"].IsString())
	{
		expDate = doc["exp"].GetString();
	}
	
	if (doc.HasMember("level") && doc["level"].IsString())
	{
		level = doc["level"].GetString();
	}

	if (doc.HasMember("userType") && doc["userType"].IsString())
	{
		userType = doc["userType"].GetString();
	}

	SqlConNode *pNode = s_sqlPool.GetFree();
	if (!pNode)
	{
		WARNNING_LOGGER << "��ȡ���нڵ�ʧ�ܣ�" << END_LOGGER;
		return;
	}
	MYSQL *pSqlCon = pNode->pSqlCon;
	if (!pSqlCon)
	{
		WARNNING_LOGGER << "��ȡ���ݿ�����ʧ�ܣ�" << END_LOGGER;
		return;
	}

	//�ȿ��Ƿ��˺��Ѵ���
	std::string sql = "SELECT NAME from user WHERE NAME=\'" + userName + "\'";
	bool existUserName = false;
	int rc = mysql_real_query(pSqlCon, sql.c_str(), sql.length());
	if (rc != 0)
	{
		WARNNING_LOGGER << "SELECT NAME user WHERE NAME= " << userName << " Failed:" << mysql_error(pSqlCon) << END_LOGGER;
		m_strResult = "1001";
		s_sqlPool.Recycle(pNode);
		return;
	}
	MYSQL_RES *res = mysql_store_result(pSqlCon);//�����������res�ṹ����
	if (mysql_fetch_row(res))
	{
		m_strResult = "1002";
		INFO_LOGGER << "�˺��Ѵ��ڣ��볢��ѡ�������˺�ע�ᣡ: " << userName << END_LOGGER;
		s_sqlPool.Recycle(pNode);
		return;
	}

	//�ٿ����������Ƿ���ڣ��Ƿ���Ч
	sql = "SELECT status from saler WHERE refer_code=\'" + referCode + "\'";
	rc = mysql_real_query(pSqlCon, sql.c_str(), sql.length());
	if (rc != 0)
	{
		WARNNING_LOGGER << sql << " Failed:" << mysql_error(pSqlCon) << END_LOGGER;
		m_strResult = "1001";
		s_sqlPool.Recycle(pNode);
		return;
	}
	res = mysql_store_result(pSqlCon);//�����������res�ṹ����
	MYSQL_ROW row = mysql_fetch_row(res);
	if (!row)
	{
		m_strResult = "1003";
		s_sqlPool.Recycle(pNode);
		return;
	}
	else
	{
		int nStatus = atoi(row[0]);
		if (nStatus != 0)
		{
			m_strResult = "1004";
			s_sqlPool.Recycle(pNode);
			return;
		}
	}

	//��������ע��
	std::string expTime = GetDefaultExp();
	//sql = "INSERT INTO USER (NAME, PASSWORD, QQCOUNT, EXP) VALUES (\'" + userName + "', '" + passWord + +"', '" + qqCount + "', '" + expTime + "')";
	sql = "INSERT INTO user (NAME, PASSWORD, QQCOUNT, EXP, LEVEL, USERTYPE, referer, register_time) VALUES (\'" + \
		userName + "', '" + passWord + "', '" + qqCount + "', '" + expTime + "', '" + level + "', '" + userType + "', '" + referCode + "',NOW())";
	rc = mysql_real_query(pSqlCon, sql.c_str(), sql.length());
	if (rc != 0)
	{
		WARNNING_LOGGER << "SELECT NAME user WHERE NAME= " << userName << " Failed:" << mysql_error(pSqlCon) << END_LOGGER;
		m_strResult = "1001";
		s_sqlPool.Recycle(pNode);
		return;
	}

	//��ȡ��ǰ�û�id
	sql = "SELECT * from user WHERE NAME=\'" + userName + "\' and PASSWORD=\'" + passWord + "\'";
	rc = mysql_real_query(pSqlCon, sql.c_str(), sql.length());
	if (rc != 0)
	{
		WARNNING_LOGGER << sql << "Failed" << mysql_error(pSqlCon) << END_LOGGER;
	}
	else
	{
		MYSQL_RES *res = mysql_store_result(pSqlCon);//�����������res�ṹ����
		MYSQL_ROW row = mysql_fetch_row(res);
		if (row)
		{
			//ע�������Ҫ�½�һ����ѡ�ɺ�����
			char szSql[256] = { 0 };
			sprintf_s(szSql, sizeof(szSql) - 1, "INSERT INTO user_block (user_id, name, sort_num, visible, internal_flag, creation_time) VALUES (\
				%d ,'%s',%d,%d,%d,NOW())", atoi(row[0]), string2UTF8("��ѡ��").c_str(), 0, 1, 1);
			sql = szSql;
			rc = mysql_real_query(pSqlCon, sql.c_str(), sql.length());
			if(rc != 0)
			{
				WARNNING_LOGGER << sql << "Failed" << mysql_error(pSqlCon) << END_LOGGER;
			}

			sprintf_s(szSql, sizeof(szSql) - 1, "INSERT INTO user_block (user_id, name, sort_num, visible, internal_flag, creation_time) VALUES (\
				%d ,'%s',%d,%d,%d,NOW())", atoi(row[0]), string2UTF8("��ʱ������").c_str(), 0, 1, 2);
			sql = szSql;
			rc = mysql_real_query(pSqlCon, sql.c_str(), sql.length());
			if (rc != 0)
			{
				WARNNING_LOGGER << sql << "Failed" << mysql_error(pSqlCon) << END_LOGGER;
			}
		}
	}

	m_strResult = "1000";
	INFO_LOGGER << "�˺�ע��ɹ�: " << userName << END_LOGGER;

	s_sqlPool.Recycle(pNode);
}

void CClientParse::ResetPassWord(Document& doc)
{
	std::string userName, oldPassWord, newPassWord;

	//�û����������
	if (doc.HasMember("userName") && doc["userName"].IsString())
	{
		userName = doc["userName"].GetString();
	}
	else
	{
		WARNNING_LOGGER << "�û���������" << END_LOGGER;
		return;
	}

	//����������
	if (doc.HasMember("oldPassWord") && doc["oldPassWord"].IsString())
	{
		oldPassWord = doc["oldPassWord"].GetString();
	}
	else
	{
		WARNNING_LOGGER << "�����벻����" << END_LOGGER;
		return;
	}

	//������������
	if (doc.HasMember("newPassWord") && doc["newPassWord"].IsString())
	{
		newPassWord = doc["newPassWord"].GetString();
	}
	else
	{
		WARNNING_LOGGER << "�����벻����" << END_LOGGER;
		return;
	}

	SqlConNode *pNode = s_sqlPool.GetFree();
	if (!pNode)
	{
		WARNNING_LOGGER << "��ȡ���нڵ�ʧ�ܣ�" << END_LOGGER;
		return;
	}
	MYSQL *pSqlCon = pNode->pSqlCon;
	if (!pSqlCon)
	{
		WARNNING_LOGGER << "��ȡ���ݿ�����ʧ�ܣ�" << END_LOGGER;
		return;
	}

	std::string sql = "SELECT PASSWORD from user WHERE NAME=\'" + userName + "\'";
	int rc = mysql_real_query(pSqlCon, sql.c_str(), sql.length());
	if (rc != 0)
	{
		WARNNING_LOGGER << "SELECT NAME user WHERE NAME= " << userName << " Failed:" << mysql_error(pSqlCon) << END_LOGGER;
		m_strResult = "5001";
		s_sqlPool.Recycle(pNode);
		return;
	}
	MYSQL_RES *res = mysql_store_result(pSqlCon);//�����������res�ṹ����
	MYSQL_ROW row = mysql_fetch_row(res);
	if (!row)
	{
		m_strResult = "5004";
		INFO_LOGGER << "�û�: " << userName << " ������" << END_LOGGER;
		s_sqlPool.Recycle(pNode);
		return;
	}

	//�ȽϿ��е�����ʹ�����������
	std::string strData = row[0];
	if (strData.compare(oldPassWord) != 0)
	{
		INFO_LOGGER << "����ľ����벻��ȷ:" << userName << END_LOGGER;
		m_strResult = "5002";
		s_sqlPool.Recycle(pNode);
		return;
	}

	//��������
	sql = "UPDATE user SET PASSWORD=\'" + newPassWord + "\'" + \
		"WHERE NAME=\'" + userName + "\'";;
	rc = mysql_real_query(pSqlCon, sql.c_str(), sql.length());
	if (rc != 0)
	{
		ERROR_LOGGER << "UPDATE user SET PASSWORD: " << userName << " Failed" << mysql_error(pSqlCon) << END_LOGGER;
		m_strResult = "5003";
	}
	m_strResult = "0";
	INFO_LOGGER << "RESET PASSWORD SUCCEED: " << userName << END_LOGGER;

	s_sqlPool.Recycle(pNode);
}

void CClientParse::GetUserLevel(Document& doc)
{
	if (m_userName.empty())
	{
		m_strResult = "-1";
		return;
	}

	SqlConNode *pNode = s_sqlPool.GetFree();
	if (!pNode)
	{
		WARNNING_LOGGER << "��ȡ���нڵ�ʧ�ܣ�" << END_LOGGER;
		return;
	}
	MYSQL *pSqlCon = pNode->pSqlCon;
	if (!pSqlCon)
	{
		WARNNING_LOGGER << "��ȡ���ݿ�����ʧ�ܣ�" << END_LOGGER;
		return;
	}

	std::string sql = "SELECT LEVEL from user WHERE NAME=\'" + m_userName + "\'";
	int rc = mysql_real_query(pSqlCon, sql.c_str(), sql.length());
	if (rc != 0)
	{
		//WARNNING_LOGGER << "SELECT LEVEL USER WHERE NAME= " << m_userName << " Failed:" << END_LOGGER;
		m_strResult = "2003";
		s_sqlPool.Recycle(pNode);
		return;
	}
	MYSQL_RES *res = mysql_store_result(pSqlCon);//�����������res�ṹ����
	MYSQL_ROW row = mysql_fetch_row(res);
	if (row == nullptr || *row == nullptr)
	{
		m_strResult = "2001";
		//INFO_LOGGER << "�û�: " << m_userName << " ������" << END_LOGGER;
		s_sqlPool.Recycle(pNode);
		return;
	}

	//�����ֵ
	m_strResult = row[0];
	INFO_LOGGER << "�û�:" << m_userName << " �ȼ�: " << m_strResult << END_LOGGER;

	s_sqlPool.Recycle(pNode);
}

void CClientParse::GetFunctionPermission(Document& doc)
{
	if (m_userName.empty() || atoi(m_userId.c_str()) <= 0)
	{
		m_strResult = "";
		return;
	}
	m_strResult = "";

	SqlConNode *pNode = s_sqlPool.GetFree();
	if (!pNode)
	{
		WARNNING_LOGGER << "��ȡ���нڵ�ʧ�ܣ�" << END_LOGGER;
		return;
	}
	MYSQL *pSqlCon = pNode->pSqlCon;
	if (!pSqlCon)
	{
		WARNNING_LOGGER << "��ȡ���ݿ�����ʧ�ܣ�" << END_LOGGER;
		return;
	}

	//select uv.user_id,uv.version_id,v.name,vf.function_id,f.name,uv.expiry_time from user_version uv,version_function vf,function f,app_version v 
	//where uv.user_id=2 and uv.version_id=vf.version_id and f.id=vf.function_id and v.id=uv.version_id
	std::string sql = "select uv.user_id,uv.version_id,v.name,vf.function_id,f.name,uv.expiry_time from user_version uv,version_function vf,function f,app_version v \
		where uv.user_id=" + m_userId + " and uv.version_id=vf.version_id and f.id=vf.function_id and v.id=uv.version_id";
	int rc = mysql_real_query(pSqlCon, sql.c_str(), sql.length());
	if (rc != 0)
	{
		//WARNNING_LOGGER << "SELECT LEVEL USER WHERE NAME= " << m_userName << " Failed:" << END_LOGGER;
		m_strResult = "-1";
		s_sqlPool.Recycle(pNode);
		return;
	}
	MYSQL_RES *res = mysql_store_result(pSqlCon);//�����������res�ṹ����
	MYSQL_ROW row;
	while (row = mysql_fetch_row(res))
	{
		m_strResult += row[3];
		m_strResult += ";";
	}

	s_sqlPool.Recycle(pNode);

	INFO_LOGGER << "�û�:" << m_userName << " ��������Ȩ��Ϊ: " << m_strResult << END_LOGGER;
}

void CClientParse::BeyondDeadLine(Document& doc)
{
	if (m_userName.empty())
	{
		m_strResult = "-1";
		return;
	}

	SqlConNode *pNode = s_sqlPool.GetFree();
	if (!pNode)
	{
		return;
	}
	MYSQL *pSqlCon = pNode->pSqlCon;
	if (!pNode || !pSqlCon)
	{
		WARNNING_LOGGER << "��ȡ���ݿ�����ʧ�ܣ�" << END_LOGGER;
		return;
	}

	std::string sql = "SELECT EXP from user WHERE NAME=\'" + m_userName + "\'";
	int rc = mysql_real_query(pSqlCon, sql.c_str(), sql.length());
	if (rc != 0)
	{
		//WARNNING_LOGGER << "SELECT LEVEL USER WHERE NAME= " << m_userName << " Failed:" << END_LOGGER;
		m_strResult = "3";
		s_sqlPool.Recycle(pNode);
		return;
	}
	MYSQL_RES *res = mysql_store_result(pSqlCon);//�����������res�ṹ����
	MYSQL_ROW row = mysql_fetch_row(res);
	if (!row)
	{
		m_strResult = "2001";
		//INFO_LOGGER << "�û�: " << m_userName << " ������" << END_LOGGER;
		s_sqlPool.Recycle(pNode);
		return;
	}

	int expDateVal = atoi(row[0]);
	SYSTEMTIME st;
	GetSystemTime(&st);
	int y = st.wYear;  //��ȡ�����
	int m = st.wMonth; //��ȡ��ǰ�·�
	int d = st.wDay;   //��ü���
	char buf[64] = { 0 };
	sprintf_s(buf, "%d%02d%02d", y, m, d);
	int curDate = atoi(buf);
	int subVal = expDateVal - curDate;
	if (subVal >= 0)
	{
		m_strResult = "0";
	}
	else
	{
		m_strResult = "1";
	}

	s_sqlPool.Recycle(pNode);
}

void CClientParse::CheckRelogin(Document& doc)
{
	if (m_bReLogin)
	{
		m_strResult = "1";
		INFO_LOGGER << "��⵽�û��ظ���½��ǿ�ƹرգ�" << m_userName << END_LOGGER;
	}
	else
	{
		m_strResult = "0";
		//INFO_LOGGER << "�ظ���¼������" << END_LOGGER;
	}
}

void CClientParse::DeleteSelfLoginState(CClientParse* pClient)
{
	CAutoLock lck(&s_clientLock);

	auto it = s_ClientMap.begin();
	for (; it != s_ClientMap.end(); it++)
	{
		if (it->second == pClient)
		{
			s_ClientMap.erase(it);
			break;
		}
	}
}

CLock CClientParse::s_clientLock;

CObjectPool<SqlConNode> CClientParse::s_sqlPool;

std::map<std::string, CClientParse*> CClientParse::s_ClientMap;


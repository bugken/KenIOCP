#ifndef __BUFFER_H__
#define __BUFFER_H__
#include "BufferSlice.h"
#include <Windows.h>
#include <string>

class Buffer
{
public:
	Buffer();
	virtual ~Buffer();
	operator Slice();
	void clear();
	//�൱��consume����consume��ͬ��removeÿ�ζ�Ҫ�����ڴ�
	UINT remove(UINT nSize);
	UINT read(PBYTE pData, UINT nSize);
	BOOL write(PBYTE pData, UINT nSize);
	BOOL write(PCHAR pData, UINT nSize);
	BOOL write(const std::string& s);
	BOOL insert(PBYTE pData, UINT nSize);
	BOOL insert(const std::string& s);
	int scan(PBYTE pScan, UINT nPos);
	void copy(Buffer& buf);
	PBYTE getBuffer(UINT nPos = 0);
	UINT getBufferLen(); //���ݴ�С
	void writeFile(const std::string& fileName);	

protected:
	UINT reallocateBuffer(UINT nSize); //���·���
	UINT deallocateBuffer(UINT nSize); //ò��û��	
	UINT getMemSize(); //ռ���ڴ��С

protected:
	PBYTE m_pBegin; //������ͷ��λ�ã��̶����ƶ�
	PBYTE m_pEnd; //������β��λ��
	UINT m_nSize; //Bufferռ���ڴ��С
};

#endif // !__BUFFER_H__



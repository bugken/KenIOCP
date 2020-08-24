#ifndef _OFFER_OBJECT_POOL_H_
#define _OFFER_OBJECT_POOL_H_

#include "Lock.h"

/** 
* ����صķ�װ���˴����������ݿ����Ӷ���
*/
template<class T> class CObjectPool
{
public:
    /** 
    * ���캯��
    */
    CObjectPool()
    {
        m_pDataHead = NULL;
        m_pDataTail = NULL;
        m_pFreeHead = NULL;
        m_pFreeTail = NULL;
        m_iTotalNum = 0;
        m_iFreeNum = 0;
        m_iDataNum = 0;
        m_hPushEvent = CreateEvent(NULL, TRUE, FALSE, NULL);	///���������¼�
        m_hFreeEvent = CreateEvent(NULL, TRUE, FALSE, NULL);	///���������¼�

		//����ʱ������Ĭ�ϸ����Ľڵ�
		Create();
    }
    
    /** 
    * ��������
    */
    virtual ~CObjectPool()
    {
		CAutoLock lck(&m_szListLock);

        Recycle(m_pDataHead);	///�����ݽڵ�ŵ����нڵ��С�
        T* pNode = m_pFreeHead;
        ///ѭ���ͷŽڵ�
        while(m_pFreeHead)
        {
            m_pFreeHead = pNode->pNext;
            delete pNode;		/**< �ͷŽڵ� */
            pNode = m_pFreeHead;
        }
        
        m_pFreeTail = NULL;
        
        ///�ͷż����¼�
        if(m_hPushEvent)
        {
            CloseHandle(m_hPushEvent);
            m_hPushEvent = 0;
        }
        
        ///�ͷſ����¼�
        if(m_hFreeEvent)
        {
            CloseHandle(m_hFreeEvent);
            m_hFreeEvent = 0;
        }
    }
    
    /** 
    * �����½ڵ�
    * @param iCount: ��Ҫ����Ľڵ����
    * @return ���سɹ�����Ľڵ����
    */
    int Create(int iCount = 4)
    {
        ///������
        if(iCount <= 0)
            return -1;
        
        T* pNode;
        int iLoop;
		CAutoLock lck(&m_szListLock);
        for(iLoop = 0; iLoop < iCount; iLoop ++)
        {
            pNode = new(std::nothrow) T();
            if(pNode == NULL)
                return iLoop;
            
            ///���뵽������
            if(m_pFreeTail)
            {
                m_pFreeTail->pNext = pNode;
                m_pFreeTail = pNode;
            }
            else
            {
                m_pFreeHead = m_pFreeTail = pNode;
            }
            m_iTotalNum ++; 
            m_iFreeNum ++;
        }
        
        return iLoop;
    }
    
    /** 
    * �������������
    * @param pNode: �ڵ�����ָ��
    * @return 0: �ɹ�������ʧ��
    */
    int Push(T* pNode)
    {
        ///��鴫��Ĳ���
        if(pNode == NULL)
            return -1;
        
		CAutoLock lck(&m_szListLock);
        if(m_pDataTail)
        {
            m_pDataTail->pNext = pNode;
            m_pDataTail = pNode;
        }
        else
        {
            m_pDataHead = m_pDataTail = pNode;
        }
        m_iDataNum ++;
        SetEvent(m_hPushEvent);
        
        return 0;
    }

    /** 
    * �Ӷ�����ȡ���ڵ�
    * @param iCount: ��Ҫȡ�������ݸ�����Ĭ��Ϊ1
    * @return �ڵ�ͷָ�룬��Ϊ�յ�ʱ�������û�����ݡ�
    */
    T* Pop(int iCount = 1)
    {
        if(iCount <= 0)
            return NULL;
        
        T* pNode = NULL; 
        T* pBefore = NULL;
        FBASE2_NAMESPACE::CAutoMutex cMutex(&m_szListLock);
        if(m_pDataHead)
        {
            pNode = m_pDataHead;
            for(int i = 0; i < iCount && m_pDataHead; i ++)
            {
                pBefore = m_pDataHead;
                m_pDataHead = m_pDataHead->pNext;
                m_iDataNum --;
            }
            if(!m_pDataHead)
            {
                m_pDataHead = m_pDataTail = NULL;
                ///������ֿգ��������¼���
                ResetEvent(m_hPushEvent);
            }
            else
            {
                pBefore->pNext = NULL;
            }
        }
        
        return pNode;
    }
    
    /** 
    * ȡ�������¼����
    * @return �¼����
    */
    HANDLE GetPushEvent() { return m_hPushEvent; }
    
    /** 
    * ȡ�������¼����
    * @return �¼����
    */
    HANDLE GetFreeEvent() { return m_hFreeEvent; }
    
    /** 
    * ȡ��һ�����нڵ�
    * @param iCount: ��Ҫȡ�õĿ��нڵ������ Ĭ��Ϊ1
    * @return ���ؿ��нڵ����
    */
    T* GetFree(int iCount = 1)
    {
        if(iCount <= 0 || !m_pFreeHead)
            return NULL;

		//���û�п��нڵ㣬��ô��һ��
		if (0 == m_iFreeNum)
		{
			//1000ms�Ȳ������нڵ㣬��ô����
			if (WaitForSingleObject(m_hFreeEvent, 1000) != WAIT_OBJECT_0)
			{
				return NULL;
			}
		}

		//����������ӣ������¼��ȴ��ʹ�����ͬһ������
		CAutoLock lck(&m_szListLock);
        
        T* pNode = m_pFreeHead;
        T* pBefore = m_pFreeHead;
        int i;
        for(i = 0; i < iCount && m_pFreeHead; i ++)
        {
            pBefore = m_pFreeHead;
            m_pFreeHead = m_pFreeHead->pNext;
            m_iFreeNum --;
        }
        if(!m_pFreeHead)
            m_pFreeTail = m_pFreeHead = NULL;
        else
            pBefore->pNext = NULL;
        
        return pNode;
    }
    
    /** 
    * ���տ��нڵ�
    * @param pNodeHead: ��Ҫ���յĿ��нڵ�ͷ
    * @return void
    */
    void Recycle(T* pNodeHead)
    {
		CAutoLock lck(&m_szListLock);

        if(!pNodeHead)
            return;
        
        if(m_pFreeTail)
        {
            m_pFreeTail->pNext = pNodeHead;
            m_pFreeTail = pNodeHead;
        }
        else
        {
            m_pFreeHead = m_pFreeTail = pNodeHead;
        }
        m_iFreeNum ++;
        while(m_pFreeTail->pNext)
        {
            m_pFreeTail = m_pFreeTail->pNext;
            m_iFreeNum ++;
        }
		//����һ�µȴ��¼�
		SetEvent(m_hFreeEvent);
    }
    
    /** 
    * ȡͳ����Ϣ
    * @param iTotalNum: �����ܵĽ����
    * @param iDataNum: �������ݽڵ���
    * @param iFreeNum: ���ؿ��нڵ���
    * @return void
    */
    void GetStat(int &iTotalNum, int &iDataNum, int &iFreeNum)
    {
        iTotalNum = m_iTotalNum;
        iDataNum = m_iDataNum;
        iFreeNum = m_iFreeNum;
    }
    
private:
    T* m_pDataHead;
    T* m_pDataTail;
    T* m_pFreeHead;
    T* m_pFreeTail;/**< ��βָ�� */
    int m_iTotalNum, m_iDataNum, m_iFreeNum; /**< �ڵ��� */
    HANDLE m_hPushEvent;    /**< ������������¼� */
    HANDLE m_hFreeEvent;   /**< ���нڵ㷵���¼� */
    
	CLock m_szListLock;/**< ������ */
};

#endif
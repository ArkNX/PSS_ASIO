#ifndef _PSS_CCONNECT_COUNTER_H
#define _PSS_CCONNECT_COUNTER_H

#include "define.h"
#include "singleton.h"

//ȫ�ּ�����
//�������в�ͬ���͵�Connect��id���ɣ���֤Ψһ��
//add by freeeyes

class CConnectCounter
{
public:
	uint32 CreateCounter();  //�õ�Ψһ����ID
	
private:
	uint32 count_index       = 1;
	mutable std::mutex _mutex;//ͬ����
};

using App_ConnectCounter = PSS_singleton<CConnectCounter>;

#endif

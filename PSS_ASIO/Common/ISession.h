#pragma once

#include "define.h"

//����IO�ӿڵ�����
//add by freeeyes

class ISession
{
public:
    ISession() = default;
    virtual ~ISession() = default;

    virtual void set_write_buffer(uint32 connect_id, const char* data, size_t length) = 0; //д��Щ����
    virtual void do_write(uint32 connect_id) = 0;        //д��IO
    virtual void do_write_immediately(uint32 connect_id, const char* data, size_t length) = 0; //����д��IO�˿�
    virtual void close(uint32 connect_id) = 0;        //�ر�IO�˿�
    virtual void add_send_finish_size(uint32 connect_id, size_t send_length) = 0;  //����д��IO�ɹ��ֽ�
};

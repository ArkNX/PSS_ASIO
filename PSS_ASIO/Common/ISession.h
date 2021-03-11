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
    virtual EM_CONNECT_IO_TYPE get_io_type() = 0; //��õ�ǰIO״̬
    virtual uint32 get_mark_id(uint32 connect_id) = 0; //��õ�ǰ���ӱ���ǵ�ID
    virtual std::chrono::steady_clock::time_point& get_recv_time() = 0;   //�õ���������ʱ��
    virtual bool format_send_packet(uint32 connect_id, CMessage_Packet& message) = 0;  //��ʽ����������
};

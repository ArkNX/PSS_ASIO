#include "TtyServer.h"

CTTyServer::CTTyServer(shared_ptr<asio::serial_port> serial_port_param, uint32 packet_parse_id, uint32 max_buffer_length)
    : packet_parse_id_(packet_parse_id), serial_port_param_(serial_port_param)
{
    //�������ӽ�����Ϣ
    session_recv_buffer_.Init(max_buffer_length);
    session_send_buffer_.Init(max_buffer_length);

    connect_client_id_ = App_ConnectCounter::instance()->CreateCounter();

    packet_parse_interface_ = App_PacketParseLoader::instance()->GetPacketParseInfo(packet_parse_id);

    App_WorkThreadLogic::instance()->add_thread_session(connect_client_id_, shared_from_this());

    _ClientIPInfo remote_ip;
    _ClientIPInfo local_ip;
    packet_parse_interface_->packet_connect_ptr_(connect_client_id_, remote_ip, local_ip, EM_CONNECT_IO_TYPE::CONNECT_IO_TTY);

    do_receive();
}

void CTTyServer::do_receive()
{
    serial_port_param_->async_read_some(asio::buffer(session_recv_buffer_.get_curr_write_ptr(), session_recv_buffer_.get_buffer_size()),
        [this](std::error_code ec, std::size_t length)
        {
            auto connect_id = connect_client_id_;

            if (!ec && length > 0)
            {
                //�������ݰ�
                auto self(shared_from_this());

                recv_data_size_ += length;

                //��������������Ͽ����ӣ����ٽ������ݡ�
                if (session_recv_buffer_.get_buffer_size() == 0)
                {
                    //���Ͽ�(���������)
                    session_recv_buffer_.move(length);
                    App_WorkThreadLogic::instance()->close_session_event(connect_id);
                    do_receive();
                }

                session_recv_buffer_.set_write_data(length);

                //�������ݲ��
                vector<CMessage_Packet> message_list;
                bool ret = packet_parse_interface_->packet_from_recv_buffer_ptr_(connect_client_id_, &session_recv_buffer_, message_list, EM_CONNECT_IO_TYPE::CONNECT_IO_TTY);
                if (!ret)
                {
                    //���ӶϿ�(����������ȷ)
                    session_recv_buffer_.move(length);
                    App_WorkThreadLogic::instance()->close_session_event(connect_id);
                    do_receive();
                }
                else
                {
                    //���ӵ����ݶ��д���
                    App_WorkThreadLogic::instance()->do_thread_module_logic(connect_id, message_list, self);
                }

                do_receive();
            }
            else
            {
                do_receive();
            }
        });
}

void CTTyServer::set_write_buffer(uint32 connect_id, const char* data, size_t length)
{
    if (session_send_buffer_.get_buffer_size() <= length)
    {
        //����Щ�����Ѿ�����
        PSS_LOGGER_DEBUG("[CTTyServer::set_write_buffer]connect_id={0} is full.", connect_id);
        return;
    }

    std::memcpy(session_send_buffer_.get_curr_write_ptr(),
        data,
        length);
    session_send_buffer_.set_write_data(length);
}

void CTTyServer::clear_write_buffer()
{
    session_send_buffer_.move(session_send_buffer_.get_write_size());
}

void CTTyServer::do_write(uint32 connect_id)
{
    PSS_UNUSED_ARG(connect_id);

    //��װ��������
    auto send_buffer = make_shared<CSendBuffer>();
    send_buffer->data_.append(session_send_buffer_.read(), session_send_buffer_.get_write_size());
    send_buffer->buffer_length_ = session_send_buffer_.get_write_size();

    //PSS_LOGGER_DEBUG("[CTTyServer::do_write]send_buffer->buffer_length_={}.", send_buffer->buffer_length_);
    clear_write_buffer();
    
    //�첽����
    auto self(shared_from_this());
    serial_port_param_->async_write_some(asio::buffer(send_buffer->data_.c_str(), send_buffer->buffer_length_),
        [self, send_buffer, connect_id](std::error_code ec, std::size_t length)
        {
            if (ec)
            {
                //��ʱ������
                PSS_LOGGER_DEBUG("[CTTyServer::do_write]write error({0}).", ec.message());
            }
            else
            {
                self->add_send_finish_size(connect_id, length);
            }
        });

    clear_write_buffer();
}

void CTTyServer::do_write_immediately(uint32 connect_id, const char* data, size_t length)
{
    PSS_UNUSED_ARG(connect_id);

    //��װ��������
    auto send_buffer = make_shared<CSendBuffer>();
    send_buffer->data_.append(data, length);
    send_buffer->buffer_length_ = length;

    //PSS_LOGGER_DEBUG("[CTTyServer::do_write]send_buffer->buffer_length_={}.", send_buffer->buffer_length_);

    //�첽����
    auto self(shared_from_this());
    serial_port_param_->async_write_some(asio::buffer(send_buffer->data_.c_str(), send_buffer->buffer_length_),
        [self, send_buffer, connect_id](std::error_code ec, std::size_t length)
        {
            if (ec)
            {
                //��ʱ������
                PSS_LOGGER_DEBUG("[CTTyServer::do_write]write error({0}).", ec.message());
            }
            else
            {
                self->add_send_finish_size(connect_id, length);
            }
        });

    clear_write_buffer();
}

void CTTyServer::add_send_finish_size(uint32 connect_id, size_t send_length)
{
    //�첽д����
    send_data_size_ += send_length;
}

void CTTyServer::close()
{
    packet_parse_interface_->packet_disconnect_ptr_(connect_client_id_, EM_CONNECT_IO_TYPE::CONNECT_IO_TTY);
}

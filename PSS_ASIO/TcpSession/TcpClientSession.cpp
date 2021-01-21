#include "TcpClientSession.h"

#include "TcpSession.h"

CTcpClientSession::CTcpClientSession(asio::io_context& io_context)
    : socket_(io_context)
{
}

void CTcpClientSession::start(uint32 server_id, uint32 buffer_size, string server_ip, uint16 server_port, uint32 packet_parse_id)
{
    server_id_ = server_id;

    session_recv_buffer_.Init(buffer_size);
    session_send_buffer_.Init(buffer_size);

    //��������
    tcp::endpoint end_point(asio::ip::address::from_string(server_ip.c_str()), server_port);
    asio::error_code connect_error;
    socket_.connect(end_point, connect_error);

    if (connect_error)
    {
        //���ӽ���ʧ��
        PSS_LOGGER_DEBUG("[CTcpClientSession::start]error({})", connect_error.message());
    }
    else
    {
        connect_id_ = App_ConnectCounter::instance()->CreateCounter();

        packet_parse_interface_ = App_PacketParseLoader::instance()->GetPacketParseInfo(packet_parse_id);

        //�������ӽ�����Ϣ
        _ClientIPInfo remote_ip;
        _ClientIPInfo local_ip;
        remote_ip.m_strClientIP = socket_.remote_endpoint().address().to_string();
        remote_ip.m_u2Port = socket_.remote_endpoint().port();
        local_ip.m_strClientIP = socket_.local_endpoint().address().to_string();
        local_ip.m_u2Port = socket_.local_endpoint().port();

        PSS_LOGGER_DEBUG("[CTcpClientSession::start]remote({0}:{1})", remote_ip.m_strClientIP, remote_ip.m_u2Port);
        PSS_LOGGER_DEBUG("[CTcpClientSession::start]local({0}:{1})", local_ip.m_strClientIP, local_ip.m_u2Port);

        packet_parse_interface_->packet_connect_ptr_(connect_id_, remote_ip, local_ip, EM_CONNECT_IO_TYPE::CONNECT_IO_SERVER_TCP);

        do_read();
    }
}

void CTcpClientSession::close(uint32 connect_id)
{
    socket_.close();

    //������շ����ֽ���
    PSS_LOGGER_DEBUG("[CTcpClientSession::Close]recv:{0}, send:{1}", recv_data_size_, send_data_size_);

    //�Ͽ�����
    packet_parse_interface_->packet_disconnect_ptr_(connect_id_, EM_CONNECT_IO_TYPE::CONNECT_IO_SERVER_TCP);

    App_WorkThreadLogic::instance()->delete_thread_session(connect_id);
}

void CTcpClientSession::set_write_buffer(uint32 connect_id, const char* data, size_t length)
{
    if (session_send_buffer_.get_buffer_size() <= length)
    {
        //����Щ�����Ѿ�����
        PSS_LOGGER_DEBUG("[CTcpSession::set_write_buffer]connect_id={} is full.", connect_id);
        return;
    }

    std::memcpy(session_send_buffer_.get_curr_write_ptr(),
        data,
        length);
    session_send_buffer_.set_write_data(length);
}

void CTcpClientSession::do_read()
{
    //��������
    auto self(shared_from_this());
    auto connect_id = connect_id_;

    //��������������Ͽ����ӣ����ٽ������ݡ�
    if (session_recv_buffer_.get_buffer_size() == 0)
    {
        //���ӶϿ�(���������)
        App_tms::instance()->AddMessage(1, [self, connect_id]() {
            self->close(connect_id);
            });
    }

    socket_.async_read_some(asio::buffer(session_recv_buffer_.get_curr_write_ptr(), session_recv_buffer_.get_buffer_size()),
        [this, self, connect_id](std::error_code ec, std::size_t length)
        {
            if (!ec)
            {
                recv_data_size_ += length;
                session_recv_buffer_.set_write_data(length);
                PSS_LOGGER_DEBUG("[CTcpClientSession::do_write]recv length={}.", length);

                //�������ݲ��
                vector<CMessage_Packet> message_list;
                bool ret = packet_parse_interface_->packet_from_recv_buffer_ptr_(connect_id_, &session_recv_buffer_, message_list, EM_CONNECT_IO_TYPE::CONNECT_IO_SERVER_TCP);
                if (!ret)
                {
                    //���ӶϿ�(����������ȷ)
                    App_WorkThreadLogic::instance()->close_session_event(connect_id_);
                }
                else
                {
                    //�����Ϣ����
                    App_WorkThreadLogic::instance()->do_thread_module_logic(connect_id_, message_list, self);
                }

                session_recv_buffer_.move(length);
                //����������
                self->do_read();
            }
            else
            {
                //���ӶϿ�
                close(connect_id);
            }
        });
}

void CTcpClientSession::do_write_immediately(uint32 connect_id, const char* data, size_t length)
{
    //��װ��������
    auto send_buffer = make_shared<CSendBuffer>();
    send_buffer->data_.append(data, length);
    send_buffer->buffer_length_ = length;

    //PSS_LOGGER_DEBUG("[CTcpClientSession::do_write]send_buffer->buffer_length_={}.", send_buffer->buffer_length_);

    //�첽����
    auto self(shared_from_this());
    asio::async_write(socket_, asio::buffer(send_buffer->data_.c_str(), send_buffer->buffer_length_),
        [self, connect_id, send_buffer](std::error_code ec, std::size_t length)
        {
            if (ec)
            {
                //��ʱ������
                PSS_LOGGER_DEBUG("[CTcpClientSession::do_write_immediately]({0}), message({1})", connect_id, ec.message());
            }
            else
            {
                self->add_send_finish_size(connect_id, length);
            }
        });
}

void CTcpClientSession::do_write(uint32 connect_id)
{
    //��װ��������
    auto send_buffer = make_shared<CSendBuffer>();
    send_buffer->data_.append(session_send_buffer_.read(), session_send_buffer_.get_write_size());
    send_buffer->buffer_length_ = session_send_buffer_.get_write_size();

    //PSS_LOGGER_DEBUG("[CTcpSession::do_write]send_buffer->buffer_length_={}.", send_buffer->buffer_length_);
    clear_write_buffer();

    //�첽����
    auto self(shared_from_this());
    asio::async_write(socket_, asio::buffer(send_buffer->data_.c_str(), send_buffer->buffer_length_),
        [self, send_buffer, connect_id](std::error_code ec, std::size_t length)
        {
            if (ec)
            {
                //��ʱ������
                PSS_LOGGER_DEBUG("[CTcpClientSession::do_write]write error({0}).", ec.message());
            }
            else
            {
                self->add_send_finish_size(connect_id, length);
            }
        });
}

void CTcpClientSession::add_send_finish_size(uint32 connect_id, size_t send_length)
{
    send_data_size_ += send_length;
}

void CTcpClientSession::clear_write_buffer()
{
    session_send_buffer_.move(session_send_buffer_.get_write_size());
}



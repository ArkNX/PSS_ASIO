#include "TcpClientSession.h"

#include "TcpSession.h"

CTcpClientSession::CTcpClientSession(asio::io_context* io_context)
    : socket_(*io_context)
{
}

bool CTcpClientSession::start(const CConnect_IO_Info& io_info)
{
    server_id_ = io_info.server_id;

    session_recv_buffer_.Init(io_info.recv_size);
    session_send_buffer_.Init(io_info.send_size);

    //��������
    tcp::endpoint end_point(asio::ip::address::from_string(io_info.server_ip.c_str()), io_info.server_port);
    asio::error_code connect_error;
    socket_.connect(end_point, connect_error);

    if (connect_error)
    {
        //���ӽ���ʧ��
        PSS_LOGGER_DEBUG("[CTcpClientSession::start]error({})", connect_error.message());
        return false;
    }
    else
    {
        connect_id_ = App_ConnectCounter::instance()->CreateCounter();

        packet_parse_interface_ = App_PacketParseLoader::instance()->GetPacketParseInfo(io_info.packet_parse_id);

        //�������ӽ�����Ϣ

        remote_ip_.m_strClientIP = socket_.remote_endpoint().address().to_string();
        remote_ip_.m_u2Port = socket_.remote_endpoint().port();
        local_ip_.m_strClientIP = socket_.local_endpoint().address().to_string();
        local_ip_.m_u2Port = socket_.local_endpoint().port();

        PSS_LOGGER_DEBUG("[CTcpClientSession::start]remote({0}:{1})", remote_ip_.m_strClientIP, remote_ip_.m_u2Port);
        PSS_LOGGER_DEBUG("[CTcpClientSession::start]local({0}:{1})", local_ip_.m_strClientIP, local_ip_.m_u2Port);

        packet_parse_interface_->packet_connect_ptr_(connect_id_, remote_ip_, local_ip_, io_type_);

        //���ӳ���ϵ
        App_WorkThreadLogic::instance()->add_thread_session(connect_id_, shared_from_this(), local_ip_, remote_ip_);

        do_read();

        return true;
    }
}

void CTcpClientSession::close(uint32 connect_id)
{
    auto self(shared_from_this());
    socket_.close();

    //������շ����ֽ���
    PSS_LOGGER_DEBUG("[CTcpClientSession::Close]recv:{0}, send:{1}", recv_data_size_, send_data_size_);

    //�Ͽ�����
    packet_parse_interface_->packet_disconnect_ptr_(connect_id_, io_type_);

    //�������ӶϿ���Ϣ
    App_WorkThreadLogic::instance()->delete_thread_session(connect_id, remote_ip_, self);
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
                bool ret = packet_parse_interface_->packet_from_recv_buffer_ptr_(connect_id_, &session_recv_buffer_, message_list, io_type_);
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
                App_WorkThreadLogic::instance()->close_session_event(connect_id_);
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

EM_CONNECT_IO_TYPE CTcpClientSession::get_io_type()
{
    return io_type_;
}

uint32 CTcpClientSession::get_mark_id(uint32 connect_id)
{
    PSS_UNUSED_ARG(connect_id);
    return server_id_;
}

void CTcpClientSession::clear_write_buffer()
{
    session_send_buffer_.move(session_send_buffer_.get_write_size());
}



#include "TcpSession.h"

CTcpSession::CTcpSession(tcp::socket socket)
    : socket_(std::move(socket))
{
}

void CTcpSession::open(uint32 packet_parse_id, uint32 recv_size, uint32 send_size)
{
    connect_id_ = App_ConnectCounter::instance()->CreateCounter();

    packet_parse_interface_ = App_PacketParseLoader::instance()->GetPacketParseInfo(packet_parse_id);

    session_recv_buffer_.Init(recv_size);

    //�������ӽ�����Ϣ
    remote_ip_.m_strClientIP = socket_.remote_endpoint().address().to_string();
    remote_ip_.m_u2Port = socket_.remote_endpoint().port();
    local_ip_.m_strClientIP = socket_.local_endpoint().address().to_string();
    local_ip_.m_u2Port = socket_.local_endpoint().port();
    packet_parse_interface_->packet_connect_ptr_(connect_id_, remote_ip_, local_ip_, io_type_);

    //����session ӳ��
    App_WorkThreadLogic::instance()->add_thread_session(connect_id_, shared_from_this(), local_ip_, remote_ip_);

    do_read();
}

void CTcpSession::close(uint32 connect_id)
{
    auto self(shared_from_this());
    socket_.close();

    //������շ����ֽ���
    PSS_LOGGER_DEBUG("[CTcpSession::Close]recv:{0}, send:{1} io_send_count:{2}", recv_data_size_, send_data_size_, io_send_count_);

    //�Ͽ�����
    packet_parse_interface_->packet_disconnect_ptr_(connect_id, io_type_);

    App_WorkThreadLogic::instance()->delete_thread_session(connect_id, remote_ip_, self);
}

void CTcpSession::do_read()
{
    //��������
    auto self(shared_from_this());

    auto connect_id = connect_id_;

    //��������������Ͽ����ӣ����ٽ������ݡ�
    if (session_recv_buffer_.get_buffer_size() == 0)
    {
        //���ӶϿ�(���������)
        App_WorkThreadLogic::instance()->close_session_event(connect_id_);
    }

    socket_.async_read_some(asio::buffer(session_recv_buffer_.get_curr_write_ptr(), session_recv_buffer_.get_buffer_size()),
        [this, self, connect_id](std::error_code ec, std::size_t length)
        {
            if (!ec)
            {
                recv_data_size_ += length;
                session_recv_buffer_.set_write_data(length);
                //PSS_LOGGER_DEBUG("[CTcpSession::do_write]recv length={}.", length);
                
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

void CTcpSession::do_write(uint32 connect_id)
{
    std::lock_guard<std::mutex> lck(send_thread_mutex_);

    if (is_send_finish == false || session_send_buffer_.size() == 0)
    {
        //�ϴη���û����ɻ����Ѿ��������
        return;
    }

    is_send_finish = false;

    //��װ��������
    auto send_buffer = make_shared<CSendBuffer>();
    send_buffer->data_ = session_send_buffer_;
    send_buffer->buffer_length_ = session_send_buffer_.size();

    //PSS_LOGGER_DEBUG("[CTcpSession::do_write]send_buffer->buffer_length_={}.", session_send_buffer_.size());
    clear_write_buffer();

    //�첽����
    auto self(shared_from_this());
    asio::async_write(socket_, asio::buffer(send_buffer->data_.c_str(), send_buffer->buffer_length_),
        [self, send_buffer, connect_id](std::error_code ec, std::size_t length)
        {
            if (ec)
            {
                //��ʱ������
                PSS_LOGGER_DEBUG("[CTcpSession::do_write]write error({0}).", ec.message());
            }
            else
            {
                self->add_send_finish_size(connect_id, length);
                
                //��������
                self->do_write(connect_id);
            }
        });
}

void CTcpSession::do_write_immediately(uint32 connect_id, const char* data, size_t length)
{
    set_write_buffer(connect_id, data, length);
    do_write(connect_id);
}

void CTcpSession::set_write_buffer(uint32 connect_id, const char* data, size_t length)
{
    std::lock_guard<std::mutex> lck(send_thread_mutex_);

    session_send_buffer_.append(data, length);
}

void CTcpSession::clear_write_buffer()
{
    session_send_buffer_.clear();
}

void CTcpSession::add_send_finish_size(uint32 connect_id, size_t send_length)
{
    std::lock_guard<std::mutex> lck(send_thread_mutex_);

    send_data_size_ += send_length;

    io_send_count_++;

    is_send_finish = true;
}

EM_CONNECT_IO_TYPE CTcpSession::get_io_type()
{
    return io_type_;
}

uint32 CTcpSession::get_mark_id(uint32 connect_id)
{
    PSS_UNUSED_ARG(connect_id);
    return 0;
}


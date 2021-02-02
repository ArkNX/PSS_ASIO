#include "UdpServer.h"

CUdpServer::CUdpServer(asio::io_context& io_context, std::string server_ip, short port, uint32 packet_parse_id, uint32 max_recv_size, uint32 max_send_size)
    : socket_(io_context, udp::endpoint(asio::ip::address_v4::from_string(server_ip), port)), packet_parse_id_(packet_parse_id), max_recv_size_(max_recv_size), max_send_size_(max_recv_size)
{
    //�������ӽ�����Ϣ
    std::cout << "[CUdpServer::do_accept](" << socket_.local_endpoint() << ") Begin Accept." << std::endl;

    session_recv_buffer_.Init(max_recv_size_);

    packet_parse_interface_ = App_PacketParseLoader::instance()->GetPacketParseInfo(packet_parse_id);

    do_receive();
}

void CUdpServer::do_receive()
{
    socket_.async_receive_from(
        asio::buffer(session_recv_buffer_.get_curr_write_ptr(), session_recv_buffer_.get_buffer_size()), recv_endpoint_,
        [this](std::error_code ec, std::size_t length)
        {
            //��ѯ��ǰ��connect_id
            auto connect_id = add_udp_endpoint(recv_endpoint_, length, max_send_size_);
            
            if (!ec && length > 0)
            {
                //�������ݰ�
                auto self(shared_from_this());

                //��������������Ͽ����ӣ����ٽ������ݡ�
                if (session_recv_buffer_.get_buffer_size() == 0)
                {
                    //���ӶϿ�(���������)
                    session_recv_buffer_.move(length);
                    App_WorkThreadLogic::instance()->close_session_event(connect_id);
                    do_receive();
                }

                session_recv_buffer_.set_write_data(length);

                //�������ݲ��
                vector<CMessage_Packet> message_list;
                bool ret = packet_parse_interface_->packet_from_recv_buffer_ptr_(connect_client_id_, &session_recv_buffer_, message_list, io_type_);
                if (!ret)
                {
                    //���ӶϿ�(����������ȷ)
                    session_recv_buffer_.move(length);
                    App_WorkThreadLogic::instance()->close_session_event(connect_id);
                    do_receive();
                }
                else
                {
                    //��ӵ����ݶ��д���
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

void CUdpServer::close(uint32 connect_id)
{
    close_udp_endpoint_by_id(connect_id);
}

void CUdpServer::set_write_buffer(uint32 connect_id, const char* data, size_t length)
{
    auto session_info = find_udp_endpoint_by_id(connect_id);

    if (session_info == nullptr || session_info->session_send_buffer_.get_buffer_size() <= length)
    {
        //����Щ�����Ѿ�����
        PSS_LOGGER_DEBUG("[CUdpServer::set_write_buffer]({})session_info is null or session_send_buffer_ is full", connect_id);
        return;
    }

    std::memcpy(session_info->session_send_buffer_.get_curr_write_ptr(),
        data,
        length);
    session_info->session_send_buffer_.set_write_data(length);
}

void CUdpServer::clear_write_buffer(shared_ptr<CUdp_Session_Info> session_info)
{
    session_info->session_send_buffer_.move(session_info->session_send_buffer_.get_write_size());
}

void CUdpServer::do_write(uint32 connect_id)
{
    auto session_info = find_udp_endpoint_by_id(connect_id);

    if (session_info == nullptr)
    {
        PSS_LOGGER_DEBUG("[CUdpServer::do_write]({}) is nullptr.", connect_id);
        return;
    }

    if (session_info->udp_state == EM_UDP_VALID::UDP_INVALUD)
    {
        clear_write_buffer(session_info);
        return;
    }

    //��װ��������
    auto send_buffer = make_shared<CSendBuffer>();
    send_buffer->data_.append(session_info->session_send_buffer_.read(), session_info->session_send_buffer_.get_write_size());
    send_buffer->buffer_length_ = session_info->session_send_buffer_.get_write_size();

    //PSS_LOGGER_DEBUG("[CUdpServer::do_write]send_buffer->buffer_length_={}.", send_buffer->buffer_length_);
    clear_write_buffer(session_info);

    auto self(shared_from_this());
    socket_.async_send_to(
        asio::buffer(send_buffer->data_.c_str(), send_buffer->buffer_length_), session_info->send_endpoint,
        [self, send_buffer, connect_id](std::error_code ec, std::size_t length)
        {
            if (ec)
            {
                //��ʱ������
                PSS_LOGGER_DEBUG("[CUdpServer::do_write]connect_id={0}, write error({1}).", connect_id, ec.message());
            }
            else
            {
                //�����¼�����ֽ���
                self->add_send_finish_size(connect_id, send_buffer->buffer_length_);
            }
        });
}

void CUdpServer::do_write_immediately(uint32 connect_id, const char* data, size_t length)
{
    auto session_info = find_udp_endpoint_by_id(connect_id);

    if (session_info == nullptr)
    {
        PSS_LOGGER_DEBUG("[CUdpServer::do_write]({}) is nullptr.", connect_id);
        return;
    }

    if (session_info->udp_state == EM_UDP_VALID::UDP_INVALUD)
    {
        clear_write_buffer(session_info);
        return;
    }

    //��װ��������
    auto send_buffer = make_shared<CSendBuffer>();
    send_buffer->data_.append(data, length);
    send_buffer->buffer_length_ = length;

    //PSS_LOGGER_DEBUG("[CUdpServer::do_write]send_buffer->buffer_length_={}.", send_buffer->buffer_length_);
    clear_write_buffer(session_info);

    auto self(shared_from_this());
    socket_.async_send_to(
        asio::buffer(send_buffer->data_.c_str(), send_buffer->buffer_length_), session_info->send_endpoint,
        [self, send_buffer, connect_id](std::error_code ec, std::size_t length)
        {
            if (ec)
            {
                //��ʱ������
                PSS_LOGGER_DEBUG("[CUdpServer::do_write_immediately]write error({0}).", ec.message());
            }
            else
            {
                //�����¼�����ֽ���
                self->add_send_finish_size(connect_id, send_buffer->buffer_length_);
            }
        });
}

uint32 CUdpServer::add_udp_endpoint(udp::endpoint recv_endpoint_, size_t length, uint32 max_buffer_length)
{
    auto f = udp_endpoint_2_id_list_.find(recv_endpoint_);
    if (f != udp_endpoint_2_id_list_.end())
    {
        //�ҵ��ˣ�����ID
        return f->second;
    }
    else
    {
        //����һ���µ�ID
        auto connect_id = App_ConnectCounter::instance()->CreateCounter();

        auto session_info = make_shared<CUdp_Session_Info>();
        session_info->send_endpoint = recv_endpoint_;
        session_info->recv_data_size_ += length;
        session_info->udp_state = EM_UDP_VALID::UDP_VALUD;
        session_info->session_send_buffer_.Init(max_buffer_length);

        udp_endpoint_2_id_list_[recv_endpoint_] = connect_id;
        udp_id_2_endpoint_list_[connect_id] = session_info;

        //����packet parse ���ӽ���
        _ClientIPInfo remote_ip;
        _ClientIPInfo local_ip;
        remote_ip.m_strClientIP = recv_endpoint_.address().to_string();
        remote_ip.m_u2Port = recv_endpoint_.port();
        local_ip.m_strClientIP = socket_.local_endpoint().address().to_string();
        local_ip.m_u2Port = socket_.local_endpoint().port();
        packet_parse_interface_->packet_connect_ptr_(connect_id, remote_ip, local_ip, io_type_);

        //���ӳ���ϵ
        App_WorkThreadLogic::instance()->add_thread_session(connect_id, shared_from_this(), local_ip, remote_ip);

        return connect_id;
    }
}

shared_ptr<CUdp_Session_Info> CUdpServer::find_udp_endpoint_by_id(uint32 connect_id)
{
    auto f = udp_id_2_endpoint_list_.find(connect_id);
    if (f != udp_id_2_endpoint_list_.end())
    {
        return f->second;
    }
    
    return nullptr;
}

void CUdpServer::close_udp_endpoint_by_id(uint32 connect_id)
{
    auto self(shared_from_this());

    auto f = udp_id_2_endpoint_list_.find(connect_id);
    if (f != udp_id_2_endpoint_list_.end())
    {
        //����packet parse �Ͽ���Ϣ
        packet_parse_interface_->packet_disconnect_ptr_(connect_id, io_type_);

        auto session_endpoint = f->second->send_endpoint;
        udp_id_2_endpoint_list_.erase(f);
        udp_endpoint_2_id_list_.erase(session_endpoint);
    }

    //ɾ��ӳ���ϵ��
    _ClientIPInfo remote_ip;
    auto end_f = udp_id_2_endpoint_list_.find(connect_id);
    if (end_f != udp_id_2_endpoint_list_.end())
    {
        remote_ip.m_strClientIP = end_f->second->send_endpoint.address().to_string();
        remote_ip.m_u2Port = end_f->second->send_endpoint.port();
    }

    App_WorkThreadLogic::instance()->delete_thread_session(connect_id, remote_ip, self);
}

void CUdpServer::add_send_finish_size(uint32 connect_id, size_t length)
{
    auto f = udp_id_2_endpoint_list_.find(connect_id);
    if (f != udp_id_2_endpoint_list_.end())
    {
        f->second->send_data_size_ += length;
    }
}

EM_CONNECT_IO_TYPE CUdpServer::get_io_type()
{
    return io_type_;
}

uint32 CUdpServer::get_mark_id(uint32 connect_id)
{
    PSS_UNUSED_ARG(connect_id);
    return 0;
}


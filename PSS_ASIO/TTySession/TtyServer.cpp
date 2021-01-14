#include "TtyServer.h"

CTTyServer::CTTyServer(shared_ptr<asio::serial_port> serial_port_param, uint32 packet_parse_id, uint32 max_buffer_length)
    : packet_parse_id_(packet_parse_id), serial_port_param_(serial_port_param)
{
    //�������ӽ�����Ϣ
    session_recv_buffer_.Init(max_buffer_length);
    session_send_buffer_.Init(max_buffer_length);

    connect_client_id_ = App_ConnectCounter::instance()->CreateCounter();

    packet_parse_interface_ = App_PacketParseLoader::instance()->GetPacketParseInfo(packet_parse_id);

    do_receive();
}

void CTTyServer::do_receive()
{
    serial_port_param_->async_read_some(asio::buffer(session_recv_buffer_.get_curr_write_ptr(), session_recv_buffer_.get_buffer_size()),
        [this](std::error_code ec, std::size_t length)
        {
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
                    do_receive();
                }

                //�������ݲ��
                vector<CMessage_Packet> message_list;
                bool ret = packet_parse_interface_->Parse_Packet_From_Recv_Buffer(connect_client_id_, &session_recv_buffer_, message_list, EM_CONNECT_IO_TYPE::CONNECT_IO_TTY);
                if (!ret)
                {
                    //���ӶϿ�(����������ȷ)
                    session_recv_buffer_.move(length);
                    do_receive();
                }

                //��ӵ����ݶ��д���
                App_tms::instance()->AddMessage(1, [self, message_list]() {
                    PSS_LOGGER_DEBUG("[CTcpSession::AddMessage]count={}.", message_list.size());
                    for (auto packet : message_list)
                    {
                        self->set_write_buffer(packet.buffer_.c_str(), packet.buffer_.size());
                    }

                    self->do_write();
                    });


                do_receive();
            }
            else
            {
                do_receive();
            }
        });
}

void CTTyServer::set_write_buffer(const char* data, size_t length)
{
    if (session_send_buffer_.get_buffer_size() <= length)
    {
        //����Щ�����Ѿ�����
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

void CTTyServer::do_write()
{
    //��װ��������
    auto send_buffer = make_shared<CSendBuffer>();
    send_buffer->data_.append(session_send_buffer_.read(), session_send_buffer_.get_write_size());
    send_buffer->buffer_length_ = session_send_buffer_.get_write_size();

    //PSS_LOGGER_DEBUG("[CTTyServer::do_write]send_buffer->buffer_length_={}.", send_buffer->buffer_length_);
    clear_write_buffer();
    
    //�첽����
    serial_port_param_->async_write_some(asio::buffer(send_buffer->data_.c_str(), send_buffer->buffer_length_),
        [this, send_buffer](std::error_code /*ec*/, std::size_t /*bytes_sent*/)
        {
            //�첽д����
            send_data_size_ += send_buffer->buffer_length_;
        });

    clear_write_buffer();
}



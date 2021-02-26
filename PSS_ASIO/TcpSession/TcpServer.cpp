#include "TcpServer.h"

CTcpServer::CTcpServer(asio::io_context& io_context, std::string server_ip, short port, uint32 packet_parse_id, uint32 max_buffer_size, uint32 max_send_size)
    : acceptor_(io_context, tcp::endpoint(asio::ip::address_v4::from_string(server_ip), port)), packet_parse_id_(packet_parse_id), max_recv_size_(max_buffer_size), max_send_size_(max_send_size)
{
    //�������ӽ�����Ϣ
    PSS_LOGGER_INFO("[CTcpServer::do_accept]({0}:{1}) Begin Accept.", 
        acceptor_.local_endpoint().address().to_string(),
        acceptor_.local_endpoint().port());

    do_accept();
}

void CTcpServer::close()
{
    acceptor_.close();
}

void CTcpServer::do_accept()
{
    acceptor_.async_accept(
        [this](std::error_code ec, tcp::socket socket)
        {
            if (!ec)
            {
                std::make_shared<CTcpSession>(std::move(socket))->open(packet_parse_id_, max_recv_size_, max_send_size_);
            }
            else
            {
                //���ͼ���ʧ����Ϣ
                App_WorkThreadLogic::instance()->add_frame_events(LOGIC_LISTEN_SERVER_ERROR,
                    0,
                    acceptor_.local_endpoint().address().to_string(),
                    acceptor_.local_endpoint().port(),
                    EM_CONNECT_IO_TYPE::CONNECT_IO_TCP);

                //����ʧ�ܣ��鿴������Ϣ
                PSS_LOGGER_INFO("[CTcpServer::do_accept]({0}{1})accept error:{2}", 
                    acceptor_.local_endpoint().address().to_string(),
                    acceptor_.local_endpoint().port(),
                    ec.message());
            }

            do_accept();
        });
}


#include "TcpServer.h"

CTcpServer::CTcpServer(asio::io_context& io_context, std::string server_ip, short port, uint32 packet_parse_id)
    : acceptor_(io_context, tcp::endpoint(asio::ip::address_v4::from_string(server_ip), port)), packet_parse_id_(packet_parse_id)
{
    //�������ӽ�����Ϣ
    std::cout << "[CTcpServer::do_accept](" << acceptor_.local_endpoint() << ") Begin Accept." << std::endl;

    do_accept();
}

void CTcpServer::do_accept()
{
    acceptor_.async_accept(
        [this](std::error_code ec, tcp::socket socket)
        {
            if (!ec)
            {
                std::make_shared<CTcpSession>(std::move(socket))->open(connect_clinet_id_++, packet_parse_id_, 10240);
            }
            else
            {
                //����ʧ�ܣ��鿴������Ϣ
                std::cout << ec.message() << std::endl;
            }

            do_accept();
        });
}


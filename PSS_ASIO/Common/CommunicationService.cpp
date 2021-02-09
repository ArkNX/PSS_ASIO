#include "CommunicationService.h"

void CCommunicationService::init_communication_service(asio::io_context* io_service_context, uint16 timeout_seconds)
{
    //��ȡ�����ļ������ӷ�����
    io_service_context_ = io_service_context;

    //��ʱ������񣬼������������ӵ�״̬��
    App_TimerManager::instance()->GetTimerPtr()->addTimer_loop(chrono::seconds(0), chrono::seconds(timeout_seconds), [this]()
        {
            run_check_task();
        });
}

bool CCommunicationService::add_connect(const CConnect_IO_Info& io_info, EM_CONNECT_IO_TYPE io_type)
{
    std::lock_guard <std::recursive_mutex> lock(mutex_);
    CCommunicationIOInfo connect_info;
    connect_info.connect_id_ = 0;
    connect_info.io_info_ = io_info;
    connect_info.io_type_ = io_type;

    io_connect(connect_info);
    return true;
}

void CCommunicationService::set_connect_id(uint32 server_id, uint32 connect_id)
{
    auto f = communication_list_.find(server_id);
    if (f != communication_list_.end())
    {
        f->second.connect_id_ = connect_id;

        if (connect_id == 0)
        {
            f->second.session_ = nullptr;

            //ɾ��ӳ���ϵ
            server_connect_id_list_.erase(connect_id);
        }
        else
        {
            //���ӳ���ϵ
            server_connect_id_list_[connect_id] = server_id;
        }
    }
}

void CCommunicationService::io_connect(CCommunicationIOInfo& connect_info)
{
    communication_list_[connect_info.io_info_.server_id] = connect_info;

    if (connect_info.io_type_ == EM_CONNECT_IO_TYPE::CONNECT_IO_TCP)
    {
        //IO��TCP
        auto tcp_client_session = make_shared<CTcpClientSession>(io_service_context_);
        if (true == tcp_client_session->start(connect_info.io_info_))
        {
            connect_info.session_ = tcp_client_session;
        }
    }
    else if(connect_info.io_type_ == EM_CONNECT_IO_TYPE::CONNECT_IO_UDP)
    {
        //IO��UDP
        auto udp_client_session = make_shared<CUdpClientSession>(io_service_context_);
        udp_client_session->start(connect_info.io_info_);
        connect_info.session_ = udp_client_session;
    }
    else if (connect_info.io_type_ == EM_CONNECT_IO_TYPE::CONNECT_IO_TTY)
    {
        //IO��TTY
        auto tty_client_session = make_shared<CTTyServer>(
            connect_info.io_info_.packet_parse_id,
            connect_info.io_info_.recv_size,
            connect_info.io_info_.send_size);
        tty_client_session->start(io_service_context_,
            connect_info.io_info_.server_ip,
            connect_info.io_info_.server_port,
            8,
            connect_info.io_info_.server_id);
        connect_info.session_ = tty_client_session;
    }
}

void CCommunicationService::close_connect(uint32 server_id)
{
    std::lock_guard <std::recursive_mutex> lock(mutex_);
    communication_list_.erase(server_id);

    auto connect_id = get_server_id(server_id);
    if (connect_id > 0)
    {
        server_connect_id_list_.erase(connect_id);
    }
}

bool CCommunicationService::is_exist(uint32 server_id)
{
    std::lock_guard <std::recursive_mutex> lock(mutex_);
    auto f = communication_list_.find(server_id);
    if (f != communication_list_.end())
    {
        return true;
    }
    else
    {
        return false;
    }
}

void CCommunicationService::run_check_task()
{
    std::lock_guard <std::recursive_mutex> lock(mutex_);
    PSS_LOGGER_DEBUG("[CCommunicationService::run_check_task]begin size={}.", communication_list_.size());

    for (auto& client_info : communication_list_)
    {
        if (client_info.second.session_ == nullptr)
        {
            //���½�������
            io_connect(client_info.second);
        }
    }

    PSS_LOGGER_DEBUG("[CCommunicationService::run_check_task]end.");
}

void CCommunicationService::close()
{
    std::lock_guard <std::recursive_mutex> lock(mutex_);

    server_connect_id_list_.clear();
    communication_list_.clear();
}

uint32 CCommunicationService::get_server_id(uint32 connect_id)
{
    std::lock_guard <std::recursive_mutex> lock(mutex_);
    auto f = server_connect_id_list_.find(connect_id);
    if (f != server_connect_id_list_.end())
    {
        return f->second;
    }
    else
    {
        return 0;
    }
}

void CCommunicationService::reset_connect(uint32 server_id)
{
    std::lock_guard <std::recursive_mutex> lock(mutex_);
    auto f = communication_list_.find(server_id);
    if (f != communication_list_.end())
    {
        io_connect(f->second);
    }
}

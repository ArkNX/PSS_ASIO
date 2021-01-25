#include "ModuleLogic.h"

void CModuleLogic::init_logic(command_to_module_function command_to_module_function, uint16 work_thread_id)
{
    modules_interface_.copy_from_module_list(command_to_module_function);
    work_thread_id_ = work_thread_id;
}

void CModuleLogic::add_session(uint32 connect_id, shared_ptr<ISession> session)
{
    sessions_interface_.add_session_interface(connect_id, session);
}

shared_ptr<ISession> CModuleLogic::get_session_interface(uint32 connect_id)
{
    return sessions_interface_.get_session_interface(connect_id);
}

void CModuleLogic::delete_session_interface(uint32 connect_id)
{
    sessions_interface_.delete_session_interface(connect_id);
}

void CModuleLogic::close()
{
    modules_interface_.close();
    sessions_interface_.close();
}

int CModuleLogic::do_thread_module_logic(const CMessage_Source& source, const CMessage_Packet& recv_packet, CMessage_Packet& send_packet)
{
    return modules_interface_.do_module_message(source, recv_packet, send_packet);
}

uint16 CModuleLogic::get_work_thread_id()
{
    return work_thread_id_;
}

void CWorkThreadLogic::init_work_thread_logic(int thread_count, config_logic_list& logic_list)
{
    //��ʼ���߳���
    thread_count_ = thread_count;

    App_tms::instance()->Init();

    //��ʼ���������
    for (auto logic_library : logic_list)
    {
        load_module_.load_plugin_module(logic_library.logic_path_, 
            logic_library.logic_file_name_, 
            logic_library.logic_param_);
    }

    //ִ���̶߳�Ӧ����
    for (int i = 0; i < thread_count; i++)
    {
        auto thread_logic = make_shared<CModuleLogic>();

        thread_logic->init_logic(load_module_.get_module_function_list(), i);

        thread_module_list_.emplace_back(thread_logic);

        //��ʼ���߳�
        App_tms::instance()->CreateLogic(i);
    }
    
}

void CWorkThreadLogic::close()
{
    //�ر��̲߳���
    App_tms::instance()->Close();

    for (auto f : thread_module_list_)
    {
        f->close();
    }

    thread_module_list_.clear();

    //�ر�ģ�����
    load_module_.Close();
}

void CWorkThreadLogic::add_thread_session(uint32 connect_id, shared_ptr<ISession> session)
{
    //session ��������
    uint16 curr_thread_index = connect_id % thread_count_;

    thread_module_list_[curr_thread_index]->add_session(connect_id, session);
}

void CWorkThreadLogic::delete_thread_session(uint32 connect_id)
{
    //session ���ӶϿ�
    uint16 curr_thread_index = connect_id % thread_count_;

    thread_module_list_[curr_thread_index]->delete_session_interface(connect_id);
}

void CWorkThreadLogic::close_session_event(uint32 connect_id)
{
    //session �ر��¼��ַ�
    uint16 curr_thread_index = connect_id % thread_count_;

    auto session = thread_module_list_[curr_thread_index]->get_session_interface(connect_id);

    App_tms::instance()->AddMessage(curr_thread_index, [session, connect_id]() {
        session->close(connect_id);
        });
}

int CWorkThreadLogic::do_thread_module_logic(const uint32 connect_id, vector<CMessage_Packet>& message_list, shared_ptr<ISession> session)
{
    //�����̵߳�Ͷ��
    uint16 curr_thread_index = connect_id % thread_count_;
    auto module_logic = thread_module_list_[curr_thread_index];

    //��ӵ����ݶ��д���
    App_tms::instance()->AddMessage(curr_thread_index, [session, connect_id, message_list, module_logic]() {
        //PSS_LOGGER_DEBUG("[CTcpSession::AddMessage]count={}.", message_list.size());
        CMessage_Source source;
        CMessage_Packet send_packet;

        source.connect_id_ = connect_id;
        source.work_thread_id_ = module_logic->get_work_thread_id();
        source.type_ = session->get_io_type();

        for (auto recv_packet : message_list)
        {
            module_logic->do_thread_module_logic(source, recv_packet, send_packet);
        }

        session->set_write_buffer(connect_id, send_packet.buffer_.c_str(), send_packet.buffer_.size());
        session->do_write(connect_id);
        });

    return 0;
}


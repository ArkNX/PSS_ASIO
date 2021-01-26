#pragma once

//������Ϣ��Ͷ�ݵ���
//add by freeeyes

#include "define.h"
#include "ISession.h"

//��¼����IP��ַ
class CSessionIOInfo
{
public:
    _ClientIPInfo local_info_;
    _ClientIPInfo romote_info_;
    shared_ptr<ISession> session_;
};

class CSessionInterface
{
public:
    CSessionInterface() = default;

    void add_session_interface(uint32 connect_id, shared_ptr<ISession> session, const _ClientIPInfo& local_info, const _ClientIPInfo& romote_info);

    shared_ptr<ISession> get_session_interface(uint32 connect_id);

    _ClientIPInfo get_session_local_ip(uint32 connect_id);

    _ClientIPInfo get_session_remote_ip(uint32 connect_id);

    void delete_session_interface(uint32 connect_id);

    void close();

private:
    using hashmapsessions = unordered_map<uint32, CSessionIOInfo>;
    hashmapsessions sessions_list_;
};
#include "LoadPacketParse.h"


bool CLoadPacketParse::LoadPacketInfo(uint32 u4PacketParseID, const std::string& packet_parse_path, const std::string& packet_parse_file)
{
    int nRet = 0;
    //��ʽ����PacketParse�ӿ�
    auto pPacketParseInfo = std::make_shared<_Packet_Parse_Info>();

    if (nullptr == pPacketParseInfo)
    {
        PSS_LOGGER_DEBUG("[CLoadPacketParse::LoadPacketInfo] pPacketParseInfo is nullptr!");
        return false;
    }

    pPacketParseInfo->m_u4PacketParseID   = u4PacketParseID;
    string strFilePath;

    strFilePath = fmt::format("{0}{1}", packet_parse_path, packet_parse_file);
    pPacketParseInfo->m_hModule = CLoadLibrary::PSS_dlopen(strFilePath.c_str(), RTLD_NOW);

    if(nullptr == pPacketParseInfo->m_hModule)
    {
        PSS_LOGGER_DEBUG("[CLoadPacketParse::LoadPacketInfo] strModuleName = {0}, pModuleInfo->hModule is nullptr({1})!", packet_parse_file, CLoadLibrary::PSS_dlerror());
        return false;
    }

    pPacketParseInfo->packet_from_recv_buffer_ptr_ = (packet_from_recv_buffer)CLoadLibrary::PSS_dlsym(pPacketParseInfo->m_hModule, "parse_packet_from_recv_buffer");

    if(nullptr == pPacketParseInfo->m_hModule || !pPacketParseInfo->packet_from_recv_buffer_ptr_)
    {
        PSS_LOGGER_DEBUG("[CLoadPacketParse::LoadPacketInfo] strModuleName = {0}, Function Parse_Packet_Head_Info is error!", packet_parse_file);
        CLoadLibrary::PSS_dlClose(pPacketParseInfo->m_hModule);
        PSS_LOGGER_DEBUG("[CLoadPacketParse::LoadPacketInfo]PacketID={0}, ret={1}.", pPacketParseInfo->m_u4PacketParseID, nRet);
        return false;
    }

    pPacketParseInfo->packet_connect_ptr_ = (packet_connect)CLoadLibrary::PSS_dlsym(pPacketParseInfo->m_hModule, "connect");

    if(nullptr == pPacketParseInfo->m_hModule || !pPacketParseInfo->packet_connect_ptr_)
    {
        PSS_LOGGER_DEBUG("[CLoadPacketParse::LoadPacketInfo] strModuleName = {0}, Function Connect is error!", packet_parse_file);
        CLoadLibrary::PSS_dlClose(pPacketParseInfo->m_hModule);
        PSS_LOGGER_DEBUG("[CLoadPacketParse::LoadPacketInfo]PacketID={0}, ret={1}.", pPacketParseInfo->m_u4PacketParseID, nRet);
        return false;
    }

    pPacketParseInfo->packet_disconnect_ptr_ = (packet_disconnect)CLoadLibrary::PSS_dlsym(pPacketParseInfo->m_hModule, "disConnect");

    if(nullptr == pPacketParseInfo->m_hModule || !pPacketParseInfo->packet_disconnect_ptr_)
    {
        PSS_LOGGER_DEBUG("[CLoadPacketParse::LoadPacketInfo] strModuleName = {0}, Function DisConnect is error!", packet_parse_file);
        CLoadLibrary::PSS_dlClose(pPacketParseInfo->m_hModule);
        PSS_LOGGER_DEBUG("[CLoadPacketParse::LoadPacketInfo]PacketID={0}, ret={1}.", pPacketParseInfo->m_u4PacketParseID, nRet);
        return false;
    }

    pPacketParseInfo->packet_load_ptr_ = (packet_load)CLoadLibrary::PSS_dlsym(pPacketParseInfo->m_hModule, "packet_load");

    if (nullptr == pPacketParseInfo->m_hModule || !pPacketParseInfo->packet_load_ptr_)
    {
        PSS_LOGGER_DEBUG("[CLoadPacketParse::LoadPacketInfo] strModuleName = {0}, Function Load is error!", packet_parse_file);
        CLoadLibrary::PSS_dlClose(pPacketParseInfo->m_hModule);
        PSS_LOGGER_DEBUG("[CLoadPacketParse::LoadPacketInfo]PacketID={0}, ret={1}.", pPacketParseInfo->m_u4PacketParseID, nRet);
        return false;
    }

    pPacketParseInfo->packet_close_ptr_ = (packet_close)CLoadLibrary::PSS_dlsym(pPacketParseInfo->m_hModule, "packet_close");

    if (nullptr == pPacketParseInfo->m_hModule || !pPacketParseInfo->packet_close_ptr_)
    {
        PSS_LOGGER_DEBUG("[CLoadPacketParse::LoadPacketInfo] strModuleName = {0}, Function Close is error!", packet_parse_file);
        CLoadLibrary::PSS_dlClose(pPacketParseInfo->m_hModule);
        PSS_LOGGER_DEBUG("[CLoadPacketParse::LoadPacketInfo]PacketID={0}, ret={1}.", pPacketParseInfo->m_u4PacketParseID, nRet);
        return false;
    }

    pPacketParseInfo->packet_set_output_ptr_ = (packet_set_output)CLoadLibrary::PSS_dlsym(pPacketParseInfo->m_hModule, "set_output");

    if (nullptr == pPacketParseInfo->m_hModule || !pPacketParseInfo->packet_set_output_ptr_)
    {
        PSS_LOGGER_DEBUG("[CLoadPacketParse::LoadPacketInfo] strModuleName = {0}, Function Set_output is error!", packet_parse_file);
        CLoadLibrary::PSS_dlClose(pPacketParseInfo->m_hModule);
        PSS_LOGGER_DEBUG("[CLoadPacketParse::LoadPacketInfo]PacketID={0}, ret={1}.", pPacketParseInfo->m_u4PacketParseID, nRet);
        return false;
    }

    //��ӵ�HashPool����
    m_objPacketParseList[pPacketParseInfo->m_u4PacketParseID] = pPacketParseInfo;

    //�����������
    pPacketParseInfo->packet_set_output_ptr_(spdlog::default_logger());

    //���ó�ʼ��
    pPacketParseInfo->packet_load_ptr_();

    return true;
}

shared_ptr<_Packet_Parse_Info> CLoadPacketParse::GetPacketParseInfo(uint32 u4PacketParseID)
{
    auto f = m_objPacketParseList.find(u4PacketParseID);

    if(m_objPacketParseList.end() == f)
    {
        //�����ӡ�������
        PSS_LOGGER_DEBUG("[CLoadPacketParse::GetPacketParseInfo]can't find u4PacketParseID({0}).", u4PacketParseID);
        return nullptr;
    }
    else
    {
        return f->second;
    }
}

void CLoadPacketParse::Close()
{
    PSS_LOGGER_DEBUG("[CLoadPacketParse::Close]Begin.");
    //���������Ѵ��ڵ�ָ��
    for_each(m_objPacketParseList.begin(), m_objPacketParseList.end(), [](const std::pair<uint32, shared_ptr<_Packet_Parse_Info>>& iter) {
        //�ر�ģ��ӿ�
        iter.second->packet_close_ptr_();
        CLoadLibrary::PSS_dlClose(iter.second->m_hModule);
        });

    m_objPacketParseList.clear();
    PSS_LOGGER_DEBUG("[CLoadPacketParse::Close]End.");
}

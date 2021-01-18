#ifndef _LOADMODULE_H
#define _LOADMODULE_H

#include <string>
#include <vector>
#include <unordered_map>
#include "LoadLibrary.hpp"
#include "singleton.h"
#include "TimeStamp.hpp"
#include "FrameObject.hpp"
#include "tms.hpp"

//����������ָ�����
using load_module_function_ptr = int(*)(IFrame_Object*, string module_param);
using unload_module_function_ptr = void(*)(void);
using do_message_function_ptr = int(*)(const uint16, CMessage_Packet&, CMessage_Packet&);
using get_module_state_function_ptr = bool(*)(uint32&);
using set_output_function_ptr = void(*)(shared_ptr<spdlog::logger>);

//����map�汾�����ݽṹ
using command_to_module_function = unordered_map<uint16, do_message_function_ptr>;

class _ModuleInfo
{
public:
    string           module_file_name_;     //ģ���ļ�����
    string           module_file_path_;     //ģ��·��
    string           module_param_;         //ģ����������
    PSS_Time_Point   load_module_time_ = CTimeStamp::Get_Time_Stamp(); //ģ�鴴��ʱ��
    Pss_Library_Handler hModule_                    = nullptr;
    load_module_function_ptr load_module_           = nullptr;
    unload_module_function_ptr unload_module_       = nullptr;
    do_message_function_ptr do_message_             = nullptr;
    get_module_state_function_ptr get_module_state_ = nullptr;
    set_output_function_ptr set_output_             = nullptr;

    _ModuleInfo() = default;
};

class CLoadModule
{
public:
    CLoadModule(void) = default;

    void Close();

    bool load_plugin_module(const string& module_file_path, const string& module_file_name, const string& module_param);
    bool unload_plugin_module(const string& module_file_name, bool is_delete);

    int  get_module_count() const;
    shared_ptr<_ModuleInfo> find_module_info(const char* pModuleName);

    //����ӿ��ṩ��ع���
    bool get_module_exist(const char* pModuleName);
    string get_module_param(const char* pModuleName);
    string get_module_file_path(const char* pModuleName);
    void get_all_module_name(vector<string>& vecModeInfo);

    //��������ͬ����ع���
    command_to_module_function& get_module_function_list();

private:
    bool load_module_info(shared_ptr<_ModuleInfo> module_info);    //��ʼ����ģ��Ľӿں�����

    void delete_module_name_list(const string& module_name);

    using hashmapModuleList = unordered_map<string, shared_ptr<_ModuleInfo>>;
    hashmapModuleList                  module_list_;
    vector<string>                     module_name_list_;               //��ǰ��������б�

    command_to_module_function command_to_module_function_;
};

#endif

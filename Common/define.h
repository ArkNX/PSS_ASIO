#pragma once

#include <math.h>
#include <type_traits>
#include <fstream>
#include <chrono>

#include "consoleoutput.hpp"

using namespace std;

//�Զ��ж�����ϵͳ
#define PLATFORM_WIN     0
#define PLATFORM_UNIX    1
#define PLATFORM_APPLE   2

#if defined(__WIN32__) || defined(WIN32) || defined(_WIN32) || defined(__WIN64__) || defined(WIN64) || defined(_WIN64)
#  define PSS_PLATFORM PLATFORM_WIN
#elif defined(__APPLE_CC__)
#  define PSS_PLATFORM PLATFORM_APPLE
#else
#  define PSS_PLATFORM PLATFORM_UNIX
#endif

//�������Ͷ���
using uint8 = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
using uint64 = uint64_t;
using int8 = int8_t;
using int16 = int16_t;
using int32 = int32_t;
using int64 = int64_t;
using float32 = float;
using float64 = double;

enum class ENUM_WHILE_STATE
{
    WHILE_STATE_CONTINUE = 0,
    WHILE_STATE_BREAK,
};

//���ӵ���������
enum class EM_CONNECT_IO_TYPE
{
    CONNECT_IO_TCP = 0,      //IO��TCP
    CONNECT_IO_UDP,          //IO��UDP
    CONNECT_IO_TTY,          //IO��TTY
    CONNECT_IO_SERVER_TCP,   //IO�Ƿ����ڼ�����TCP
    CONNECT_IO_SERVER_UDP,   //IO�Ƿ����ڼ�����UDP
    CONNECT_IO_FRAME,        //���Բ����Ļص�
    COMMAND_UPDATE,          //���Բ������  
    WORKTHREAD_CLOSE         //�رյ�ǰ�����߳�
};

class CMessage_Source
{
public:
    uint16 work_thread_id_ = 0;
    uint32 connect_id_ = 0;
    uint32 connect_server_id_ = 0;
    EM_CONNECT_IO_TYPE type_ = EM_CONNECT_IO_TYPE::CONNECT_IO_SERVER_TCP;
};

class CMessage_Packet
{
public:
    string buffer_;
    uint16 command_id_;
};

//�ͻ���IP��Ϣ
class _ClientIPInfo
{
public:
    string  m_strClientIP;      //�ͻ��˵�IP��ַ
    uint16 m_u2Port  = 0;        //�ͻ��˵Ķ˿�
};

//���������
#define PSS_LOGGER_DEBUG(...) SPDLOG_LOGGER_DEBUG(spdlog::default_logger(), __VA_ARGS__);
#define PSS_LOGGER_INFO(...) SPDLOG_LOGGER_INFO(spdlog::default_logger(), __VA_ARGS__);
#define PSS_LOGGER_WARN(...) SPDLOG_LOGGER_WARN(spdlog::default_logger(), __VA_ARGS__);
#define PSS_LOGGER_ERROR(...) SPDLOG_LOGGER_ERROR(spdlog::default_logger(), __VA_ARGS__);

//������Ϣ����
enum class ENUM_LOGIC_COMMAND
{
    LOGIC_COMMAND_CONNECT = 0x0001,
    LOGIC_COMMAND_DISCONNECT = 0x0002,
};

//�ݲ�ʹ�õĲ���
template <typename T>
void PSS_UNUSED_ARG(T&&)
{ }

inline void Init_Console_Output(bool blTurnOn, int nFileCount, int nLogFileMaxSize, string strConsoleName, string strLevel)
{
    Console_Output_Info obj_Console_Output_Info;
    obj_Console_Output_Info.m_blTunOn = blTurnOn;

    obj_Console_Output_Info.m_nFileCount = nFileCount;
    obj_Console_Output_Info.m_nLogFileMaxSize = nLogFileMaxSize;
    obj_Console_Output_Info.m_strConsoleName = strConsoleName;
    obj_Console_Output_Info.m_strLevel = strLevel;

    app_ConsoleOutput::instance()->Init(obj_Console_Output_Info);
}

#pragma once

#include "define.h"
#include "ISessionService.h"

//�������Ҫ�����Ҫʹ�õĶ���

class IFrame_Object
{
public:
    virtual bool Regedit_command(uint16 command_id) = 0;
    virtual ISessionService* get_session_service() = 0;
};
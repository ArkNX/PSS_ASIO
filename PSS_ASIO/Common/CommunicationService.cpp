#include "CommunicationService.h"

void CCommunicationService::init_communication_service(asio::io_context& io_service_context)
{
    //��ȡ�����ļ������ӷ�����

    //���Զ�ʱ��
    App_TimerManager::instance()->GetTimerPtr()->addTimer_loop(chrono::seconds(2), [this]()
        {
            run_check_task();
        });
}

void CCommunicationService::run_check_task()
{
    PSS_LOGGER_DEBUG("[CCommunicationService::run_check_task]ok");
}

#ifndef _NETTHREAD_SESSION_H
#define _NETTHREAD_SESSION_H

#include <string>

#include "msgqueue.h"
#include "LogicNetSession.h"
#include "NetSession.h"
#include "EventLoop.h"

using namespace std;

/*������㣨�̣߳��¼�ת��Ϊ��Ϣ��������Ϣ���У��ṩ���߼��̴߳���*/

/*������Ϣ����*/
enum Net2LogicMsgType
{
    Net2LogicMsgTypeNONE,
    Net2LogicMsgTypeEnter,
    Net2LogicMsgTypeData,
    Net2LogicMsgTypeClose,
    Net2LogicMsgTypeCompleteCallback,
};

/*�����̵߳��߼��̵߳�������Ϣ�ṹ*/
class Net2LogicMsg
{
public:
    Net2LogicMsg(){}

    Net2LogicMsg(BaseLogicSession::PTR session, Net2LogicMsgType msgType) : mSession(session), mMsgType(msgType)
    {}

    void                setData(const char* data, size_t len)
    {
        mPacket.append(data, len);
    }

    BaseLogicSession::PTR   mSession;
    Net2LogicMsgType        mMsgType;
    std::string             mPacket;
    DataSocket::PACKED_SENDED_CALLBACK mSendCompleteCallback;
};

/*  ��չ���Զ�������Ự�������¼�����������ṹ����Ϣ���͵��߼��߳�    */
class ExtNetSession : public BaseNetSession
{
public:
    ExtNetSession(BaseLogicSession::PTR logicSession) : BaseNetSession(), mLogicSession(logicSession)
    {
    }

protected:
    void            pushDataMsgToLogicThread(const char* data, size_t len);
private:
    virtual void    onEnter() final;
    virtual void    onClose() final;

protected:
    BaseLogicSession::PTR   mLogicSession;
};

void pushDataMsg2LogicMsgList(BaseLogicSession::PTR, const char* data, size_t len);
void pushCompleteCallback2LogicMsgList(const DataSocket::PACKED_SENDED_CALLBACK& callback);
void syncNet2LogicMsgList(EventLoop& eventLoop);
void procNet2LogicMsgList();

#endif
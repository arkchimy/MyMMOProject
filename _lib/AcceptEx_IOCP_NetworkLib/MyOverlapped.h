#pragma once
#include <cstdint>
#include "Common.h"
#include "NetConfig.h"
namespace network
{
enum class eComplete
{
    COMPLETE_ACCEPT,
    COMPLETE_RECV,
    COMPLETE_SEND,
    COMPLETE_RELEASE,
    NONE,
};

class MyOverlapped : public OVERLAPPED
{
  public:
    MyOverlapped(const eComplete mode) : mMode(mode) {}

    MyOverlapped(const MyOverlapped &) = delete;
    MyOverlapped &operator=(const MyOverlapped &) = delete;
    MyOverlapped(MyOverlapped &&) = delete;
    MyOverlapped &operator=(MyOverlapped &&) = delete;

    const eComplete GetMode() const { return mMode; }

  private:
    const eComplete mMode;
};
class AcceptOv final : public MyOverlapped
{
    friend class NetworkLib;
  public:
    AcceptOv(void *session)
        : MyOverlapped(eComplete::COMPLETE_ACCEPT),
          mSession(session) {}

    AcceptOv(const AcceptOv &) = delete;
    AcceptOv &operator=(const AcceptOv &) = delete;
    AcceptOv(AcceptOv &&) = delete;
    AcceptOv &operator=(AcceptOv &&) = delete;

  private:
    void *mSession;
};
class RecvOv final : public MyOverlapped
{
  public:
    RecvOv()
        : MyOverlapped(eComplete::COMPLETE_RECV) {}

    RecvOv(const RecvOv &) = delete;
    RecvOv &operator=(const RecvOv &) = delete;
    RecvOv(RecvOv &&) = delete;
    RecvOv &operator=(RecvOv &&) = delete;
};
class SendOv final : public MyOverlapped
{
    friend class NetworkLib;
    friend class Session;

  public:
    SendOv()
        : MyOverlapped(eComplete::COMPLETE_SEND) ,
          mSendMsgs{0},
          mMsgCnt(0)
    {}

    SendOv(const SendOv &) = delete;
    SendOv &operator=(const SendOv &) = delete;
    SendOv(SendOv &&) = delete;
    SendOv &operator=(SendOv &&) = delete;

  private:
    void *mSendMsgs[CONFIG_SEND_MESSAGE_MAXCOUNT];
    int16_t mMsgCnt;
};
class ReleaseOv final : public MyOverlapped
{
  public:
    ReleaseOv()
        : MyOverlapped(eComplete::COMPLETE_RELEASE) {}

    ReleaseOv(const ReleaseOv &) = delete;
    ReleaseOv &operator=(const ReleaseOv &) = delete;
    ReleaseOv(ReleaseOv &&) = delete;
    ReleaseOv &operator=(ReleaseOv &&) = delete;
};
} // namespace network
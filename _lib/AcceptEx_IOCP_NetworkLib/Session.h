#pragma once
#include <queue>
#include <mutex>

#include "NetConfig.h"
#include "MyOverlapped.h"
#include "utility/MyRingBuffer.h"
#include "utility/Message.h"

namespace network
{
using ull = unsigned long long;
using seqAddrType = __int64;

struct SeqAndIdx
{
    union
    {
        struct
        {
            __int64 Idx : 17; // sessions 의 idx
            __int64 Seq : 47; // session의 고유성을 보장하기위한 seqNumber
        };
        __int64 Value;
    };
    bool operator==(const SeqAndIdx &other) const
    {
        return this->Value == other.Value;
    }
    bool operator!=(const SeqAndIdx &other) const
    {
        return this->Value != other.Value;
    }
};

class Session
{
    friend class NetworkLib;

  public:
    Session();
    ~Session();
    void EnQueueMsg(utility::Message &msg);

    utility::Message *DeQueueMsgOrNull();

    void ReleaseSession();

    inline void SetmPtr(void* ptr) { mPtr = ptr; }
    inline void* GetmPtr() { return mPtr; }

  private:
    SOCKET mSock;
    SeqAndIdx mSessionID;

    char *mAcceptBuf;

    AcceptOv *mAcceptOv;
    RecvOv *mRecvOv;
    SendOv *mSendOv;
    ReleaseOv *mReleaseOv;

    short mIOcnt;
    char mLive;
    char mSendFlag;
    bool mAccepted; // onAccept까지 실제로 불렸는지 (phantom accept와 구분용)

    utility::MyRingBuffer* mRecvBuffer;


    std::queue<utility::Message *> mSendQ;
    std::shared_mutex mSendQlock;

    short mSenqQSize;
    void* mPtr;
};

} // namespace network

/*
   // INFO : AcceptBuffer의 설명
   -------------   char mAcceptBuf[(sizeof(SOCKADDR_IN) + 16)  * 2];      -------------
  //AcceptEx의 buffer는 두 역할입니다:                                                                                      - 로컬 주소 저장
  //- 원격(클라이언트) 주소 저장
  //크기는 sizeof(SOCKADDR_IN) 만으로는 부족합니다. AcceptEx가 내부적으로 +16바이트 여유를 요구합니다

  //// 필요한 버퍼 크기
  //// dwReceiveDataLength = 0 이므로 주소 두 개만
  //(sizeof(SOCKADDR_IN) + 16) * 2   // = 64바이트
*/
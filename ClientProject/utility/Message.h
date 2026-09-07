#pragma once

#include <cstdint>
#include <exception>
#include <iostream>

#include <concepts>
#include <string>
#include <type_traits>

#include "Common.h"

namespace utility
{

using SerializeBufferSize = uint32_t;
using ull = unsigned long long;

enum eBufferSize
{
    // 서버 FieldServer(_lib/AcceptEx_IOCP_NetworkLib) 쪽과 동일하게 상향.
    // 랭킹 TOP200 응답(약 5.6KB)을 받으려면 기존 3000으로는 부족.
    BufferSize = 6000,
    MaxSize = 6000,
};
enum eTag
{
    NORMAL,
    ENCODE,
    DECODE,
    ENCODE_BEFORE,
    DECODE_BEFORE,
    Error,
    MAX,
};

class MessageException final : public std::exception
{
  public:
    enum class eErrorType
    {
        HasNotData,
        NotEnoughSpace
    };

    MessageException(eErrorType type, const std::string &msg)
        : mType(type), mMsg(msg) {}

    virtual const char *what() const noexcept override
    {
        return mMsg.c_str();
    }

    eErrorType type() const noexcept { return mType; }

  private:
    eErrorType mType;
    std::string mMsg;
};

template <typename T>
concept Fundamental = std::is_fundamental_v<T>;
class Message final
{
  public:
    Message();
    Message(const Message &) = delete;
    Message(Message &&) = delete;

    Message &operator=(const Message &) = delete;
    Message &operator=(Message &&) = delete;

    ~Message();

  public:
    void InitMessage(int64_t sessionID,int8_t randKey);

    template <Fundamental T>
    Message &operator<<(const T data)
    {
        if (mEnd < mRearPtr + sizeof(data))
        {
            if (mSize == (DWORD)eBufferSize::BufferSize && (size_t)mRearPtr + sizeof(data) < (size_t)eBufferSize::MaxSize)
            {
                Resize();
                HexLog(eTag::Error);
            }
            else
            {
                HexLog(eTag::Error);
                throw MessageException(MessageException::eErrorType::NotEnoughSpace, "Buffer is fulled\n");
            }
        }

        memcpy(mRearPtr, &data, sizeof(data));
        mRearPtr = mRearPtr + sizeof(data);

        return *this;
    }

    Message &operator<<(char *const str)
    {
        size_t len = strlen(str);
        if (mEnd < mRearPtr + len)
        {
            if (mSize == (DWORD)eBufferSize::BufferSize && size_t(mRearPtr + len) < (size_t)eBufferSize::MaxSize)
            {
                Resize();
                HexLog(eTag::Error);
            }
            else
            {
                HexLog(eTag::Error);
                throw MessageException(MessageException::eErrorType::NotEnoughSpace, "Buffer is fulled\n");
            }
        }
        memcpy(mRearPtr, str, len);
        mRearPtr = mRearPtr + len;

        return *this;
    }

    template <Fundamental T>
    Message &operator>>(T &data)
    {
        size_t len = sizeof(T);
        if (mFrontPtr + len > mRearPtr)
        {
            throw MessageException(MessageException::eErrorType::HasNotData, "false Packet \n");
        }

        memcpy(&data, mFrontPtr, sizeof(data));
        mFrontPtr = mFrontPtr + sizeof(data);
        return *this;
    }

    Message &operator>>(char *const str)
    {
        size_t len = mRearPtr - mFrontPtr;
        if (mFrontPtr > mRearPtr)
        {
            throw MessageException(MessageException::eErrorType::HasNotData, "false Packet \n");
        }

        memcpy(str, mFrontPtr, len);
        mFrontPtr = mFrontPtr + len;
        return *this;
    }

    void EnCoding(char FK);
    bool DeCoding(char FK);

    SSIZE_T PutData(PVOID src, SerializeBufferSize size);
    SSIZE_T GetData(PVOID desc, SerializeBufferSize size);
    int64_t GetOwnerID() const { return mOwnerID; }
    int8_t GetRandomKey() const { return mRandKey; }
    char *GetFrontPtr() const { return mFrontPtr; }

    BOOL Resize();
    void Peek(char *out, SerializeBufferSize size) const;

    void HexLog(eTag tag = eTag::NORMAL, const wchar_t *filename = L"SerializeBuffer_hex.txt") const;
    size_t GetUseSize() const;
  private:
    char mBegin[(DWORD)eBufferSize::MaxSize]{0};
    char *mEnd = nullptr;

    char *mFrontPtr = nullptr;
    char *mRearPtr = nullptr;

    int64_t mOwnerID = 0;
    LONG64 mUseCnt = 0;

    int8_t mFixedKey = 0x00;
    int8_t mRandKey = 0x00;

    bool mBLastMessage = false;
    DWORD mSize = (DWORD)eBufferSize::BufferSize;
};

} // namespace utility
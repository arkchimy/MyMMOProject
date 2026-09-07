#pragma once
#define WIN32_LEAN_AND_MEAN

#include <cstdint>

#include <list>
#include <stack>
#include <string>

#include <Windows.h>

class CProfileRegistry final
{
  private:
    CProfileRegistry()
    {
        InitializeSRWLock(&mSrwManager);
    }
    ~CProfileRegistry() {}

  public:
    CProfileRegistry(const CProfileRegistry &) = delete;
    CProfileRegistry &operator=(const CProfileRegistry &) = delete;
    CProfileRegistry(CProfileRegistry &&) = delete;
    CProfileRegistry &operator=(CProfileRegistry &&) = delete;

    static CProfileRegistry &GetInstance()
    {
        static CProfileRegistry instance;
        return instance;
    }
    void CreateProfile(const wchar_t *fileName);
    void ResetEntry() noexcept;
    void RegistProfiler(class CProfileManager *manager) noexcept
    {
        _Acquires_exclusive_lock_(&mSrwManager);
        mManagerList.push_back(manager);
        _Releases_exclusive_lock_(&mSrwManager);
    }

  private:
    SRWLOCK mSrwManager;
    std::list<class CProfileManager *> mManagerList;
};

class CProfileManager
{
  public:
    CProfileManager();

    CProfileManager(const CProfileManager &) = delete;
    CProfileManager &operator=(const CProfileManager &) = delete;
    CProfileManager(CProfileManager &&) = delete;
    CProfileManager &operator=(CProfileManager &&) = delete;

  public:
    void UpdateEntry(const wchar_t *tag, int64_t distanceTime) noexcept
    {
        ProfileEntry *entry = nullptr;
        int32_t idx = 0;

        for (int32_t i = 0; i < (int32_t)eConfig::EntryMaxSize; i++)
        {
            if (mEntrys[i].mTag == tag)
            {
                entry = &mEntrys[i];
                break;
            }
        }
        if (entry == nullptr)
        {
            if (mFreeIndex.empty())
            {
                __debugbreak();
            }
            idx = mFreeIndex.top();
            mFreeIndex.pop();
            mEntrys[idx].mTag = tag;

            entry = &mEntrys[idx];
        }

        entry->mTotalCnt++;
        entry->mTotalTime += distanceTime;

        if (distanceTime < entry->mMin[(int32_t)eConfig::AbnormalBufferSize - 1])
        {
            for (int32_t i = 0; i < (int32_t)eConfig::AbnormalBufferSize; i++)
            {
                if (entry->mMin[i] > distanceTime)
                {
                    for (int j = (int32_t)eConfig::AbnormalBufferSize - 1; j != i; j--)
                    {
                        entry->mMin[j] = entry->mMin[j - 1];
                    }
                    entry->mMin[i] = distanceTime;
                    break;
                }
            }
        }
        if (entry->mMax[(int32_t)eConfig::AbnormalBufferSize - 1] < distanceTime)
        {
            for (int32_t i = 0; i < (int32_t)eConfig::AbnormalBufferSize; i++)
            {
                if (entry->mMax[i] < distanceTime)
                {
                    for (int j = (int32_t)eConfig::AbnormalBufferSize - 1; j != i; j--)
                    {
                        entry->mMax[j] = entry->mMax[j - 1];
                    }
                    entry->mMax[i] = distanceTime;
                    break;
                }
            }
        }
    }
    void CreateProfile(const wchar_t *fileName);
    void ResetEntry() noexcept;

  private:
    enum class eConfig : int64_t
    {
        EntryMaxSize       = 30,
        AbnormalBufferSize = 3,
        Max,
    };

    class ProfileEntry
    {
      public:
        ProfileEntry()
            : mTag(nullptr), mTotalTime(0), mTotalCnt(0), mMax{0}, mAvg(0.0)
        {
            for (int64_t i = 0; i < (int64_t)eConfig::AbnormalBufferSize; i++)
            {
                mMin[i] = INT_MAX;
            }
        }

        ProfileEntry(const ProfileEntry &) = delete;
        ProfileEntry &operator=(const ProfileEntry &) = delete;
        ProfileEntry(ProfileEntry &&) = delete;
        ProfileEntry &operator=(ProfileEntry &&) = delete;

        const wchar_t *mTag;
        int64_t mTotalTime;
        int64_t mTotalCnt;
        int64_t mMin[(int)eConfig::AbnormalBufferSize];
        int64_t mMax[(int)eConfig::AbnormalBufferSize];

        double mAvg;
        void InitEntry()
        {
            mTotalTime = 0;
            mTotalCnt  = 0;

            for (int64_t i = 0; i < (int64_t)eConfig::AbnormalBufferSize; i++)
            {
                mMin[i] = INT_MAX;
                mMax[i] = 0;
            }
        }
    };

    ProfileEntry mEntrys[(int)eConfig::EntryMaxSize];
    std::stack<int32_t> mFreeIndex;

    LARGE_INTEGER mQpcFrequency;
    DWORD mThreadId;
};

class Profile
{
  public:
    Profile(const wchar_t *tag)
        : mTag(tag), mStartTime{0}
    {
#ifdef PROFILE
        profileStart(&mStartTime);
#endif
    }
    ~Profile()
    {
#ifdef PROFILE
        profileEnd(mTag, mStartTime);
#endif
    }

    Profile(const Profile &) = delete;
    Profile &operator=(const Profile &) = delete;
    Profile(Profile &&) = delete;
    Profile &operator=(Profile &&) = delete;

  private:
    void profileStart(LARGE_INTEGER *out) noexcept
    {
        QueryPerformanceCounter(out);
    }
    void profileEnd(const wchar_t *tag, LARGE_INTEGER startTime);

    LARGE_INTEGER mStartTime;
    const wchar_t *mTag;
};

using Profiler = Profile;

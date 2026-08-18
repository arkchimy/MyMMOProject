#pragma once
#include <mysql.h>
#include <string>
#include <exception>
#include <unordered_map>

#pragma comment(lib, "libmysql.lib")
enum
{
    CONFIG_MAX_ERROR_LEN = 512 ,
    CONFIG_MAX_CONTEXT_LEN = 200
};

struct stResultSet
{
    friend class CDB;
    ~stResultSet()
    {
        if (mResult != nullptr)
        {
            mysql_free_result(mResult);
        }
    }
    bool Fetch();
    const char* GetValue(const char* column) const;

private:
    MYSQL_RES* mResult = nullptr;
    MYSQL_ROW mRow{};
    std::unordered_map<std::string, unsigned int> mColumnIndex;
};

class CDBException : public std::exception
{
public:
    CDBException(unsigned int errNo, const char* msg) : mErrNo(errNo)
    {
        strncpy_s(mMsg, msg, sizeof(mMsg) - 1);
    }
    const char* what() const noexcept override { return mMsg; }
    unsigned int code() const { return mErrNo; }
private:
    unsigned int mErrNo;
    char mMsg[CONFIG_MAX_ERROR_LEN];
};

class CDB
{
private:
    void myAssert(const bool x, const char* str) const;
public:
    CDB();
    ~CDB() { Disconnect(); }
public:
    bool Connect(const char* host, const char* user, const char* pass, const char* dbName, int port);
    void Disconnect();
    bool Execute(const char* query);
    bool Query(const char* query, stResultSet& out);

    void ClearError() { bFailed = false; }
    bool IsFailed() const { return bFailed; }

private:
    void fail(const char* context);
    void throwIfFailed();
    void writeLog() const;
    void writeLog(const char* fileName) const;

private:
    MYSQL* mConn = nullptr;

    bool bFailed = false;
    unsigned int mLastErrNo = 0;
    char mLastError[CONFIG_MAX_ERROR_LEN] = {};
    std::string mFilename;
};
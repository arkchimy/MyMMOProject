#include "CDB.h"
#include <cstdio>
#include <cstring>

#include <iostream>
#include <fstream>
#include <iomanip>

void CDB::myAssert(const bool x, const char* str) const
{
	if (!x)
	{
		std::ofstream outFile(mFilename.c_str(), std::ios::out | std::ios::app);
		outFile << str << "\n";

		outFile.close();

		__debugbreak();
	}
}
void CDB::throwIfFailed()
{
	if (bFailed)
	{
		writeLog("unhandleError.txt");
		throw CDBException(mLastErrNo, mLastError);
	}
}

void CDB::writeLog() const
{
	writeLog(mFilename.c_str());
}

void CDB::writeLog(const char* fileName) const
{
	std::ofstream outFile(fileName, std::ios::out | std::ios::app);

	outFile << " ErrNo : " << std::setw(5) << mLastErrNo << "\t" << mLastError << "\n";
	outFile.close();
}

void CDB::fail(const char* context)
{
	bFailed = true;
	mLastErrNo = mysql_errno(mConn);
	snprintf(mLastError, CONFIG_MAX_ERROR_LEN, "context : %s  \t errMsg : %s ", context, mysql_error(mConn));
	writeLog();
}

CDB::CDB()
{
	SYSTEMTIME stNowTime;
	GetLocalTime(&stNowTime);

	wchar_t* threadDescription = nullptr;
	HRESULT hr = GetThreadDescription(GetCurrentThread(), &threadDescription);
	myAssert(wcslen(threadDescription) != 0, "SetThreadDescription 미호출");

	std::wstring temp = threadDescription;
	LocalFree(threadDescription);

	char buf[256];
	size_t converted;
	wcstombs_s(&converted, buf, sizeof(buf), temp.c_str(), _TRUNCATE);
	std::string threadDesc(buf);

	mFilename = "Error_CDB_";
	mFilename += threadDesc;
	mFilename += std::to_string(stNowTime.wYear);
	mFilename += "_";
	mFilename += std::to_string(stNowTime.wMonth);
	mFilename += "_";
	mFilename += std::to_string(stNowTime.wDay);
	mFilename += ".txt";
}

bool CDB::Connect(const char* host, const char* user, const char* pass, const char* dbName, int port)
{
	constexpr const char* Format = "Connect(host=%s, port=%d, db=%s)";
	throwIfFailed();

	mConn = mysql_init(nullptr);
	myAssert(mConn != nullptr, "mConn = mysql_init(nullptr)  [ mConn == nullptr ]");

	MYSQL* ret = mysql_real_connect(mConn, host, user, pass, dbName, port, nullptr, 0);
	if (ret == nullptr)
	{
		char context[CONFIG_MAX_CONTEXT_LEN];
		snprintf(context, CONFIG_MAX_CONTEXT_LEN, Format, host, port, dbName);
		fail(context);
		mysql_close(mConn);
		mConn = nullptr;
		return false;
	}

	mConn = ret;
	return true;
}

void CDB::Disconnect()
{
	throwIfFailed();
	if (mConn != nullptr)
	{
		mysql_close(mConn);
		mConn = nullptr;
	}
}

bool CDB::Execute(const char* query)
{
	throwIfFailed();

	if (mysql_query(mConn, query) != 0)
	{
		fail(query);
		return false;
	}
	return true;
}

bool CDB::Query(const char* query, stResultSet& out)
{
	throwIfFailed();

	if (mysql_query(mConn, query) != 0)
	{
		fail(query);
		return false;
	}

	out.mResult = mysql_store_result(mConn);
	if (out.mResult == nullptr)   // SELECT인데 결과셋 자체가 없으면 진짜 에러
	{
		fail(query);
		return false;
	}

	MYSQL_FIELD* fields = mysql_fetch_fields(out.mResult);
	unsigned int fieldCnt = mysql_num_fields(out.mResult);
	for (unsigned int i = 0; i < fieldCnt; i++)
	{
		out.mColumnIndex.insert({ fields[i].name, i });
	}

	return true;
}

bool stResultSet::Fetch()
{
	mRow = mysql_fetch_row(mResult);
	return mRow != nullptr;
}

const char* stResultSet::GetValue(const char* column) const
{
	auto iter = mColumnIndex.find(column);
	if (iter == mColumnIndex.end())
	{
		return nullptr;
	}
	return mRow[iter->second];
}
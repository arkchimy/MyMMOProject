// CDB.cpp : 정적 라이브러리를 위한 함수를 정의합니다.
//

#include "framework.h"

void fnCDB()
{
	//    # CDB 사용법
	// 0. 스레드 이름 먼저 설정 (에러 로그 파일명에 씀, 안 하면 빈 이름으로 찍힘)
	SetThreadDescription(GetCurrentThread(), L"mainThread");

	// 1. 연결 — 반환값 반드시 체크
	CDB db;
	if (!db.Connect("localhost", "root", "123123", "accountdb", 3306))
	{
		db.ClearError();   // 처리했다는 표시. 안 하면 다음 호출에서 예외 던짐
		return;
	}

	// 2. Execute() — INSERT/UPDATE/DELETE/CREATE TABLE (결과 행 없음) — 반환값 체크
	if (!db.Execute("INSERT INTO TestTable (id, value) VALUES (1, 100) "
		"ON DUPLICATE KEY UPDATE value = 100"))
	{
		db.ClearError();
	}

	// 3. Query() — SELECT 전용 — 반환값 체크
	stResultSet rs;
	if (db.Query("SELECT id, value FROM TestTable", rs))
	{
		while (rs.Fetch())                     // 다음 행으로 이동, 더 없으면 false
		{
			const char* id = rs.GetValue("id");       // 항상 문자열로 옴
			const char* value = rs.GetValue("value"); // atoi/atoll/atof로 직접 변환
		}
	}
	else
	{
		db.ClearError();
	}

	// 4. 쿼리 문자열은 snprintf로 직접 조립 (파라미터 바인딩 없음, 숫자만 넣을 것)
	char query[256];
	snprintf(query, sizeof(query), "SELECT x, y FROM Characters WHERE account_no = %lld", (int64_t)32);
	//                                                    ^^^^ int64_t는 %lld (%d 쓰면 값 깨짐)
	//   float → %f   /   int → %d   /   %s에 외부 입력 넣지 말 것(이스케이프 없음)

	// ## 반환값을 체크 안 하면 ? → 예외
	/* 
		`Execute`/`Query`/`Connect`가 실패하면 그 자리에서 예외를 던지는 게 아니라
		`false`만 리턴한다.
		이걸 보고도 `ClearError()` 안 하고** 다음 CDB 함수를 또 호출하면**,
		그 함수 진입하자마자 이전 실패로 `CDBException`이 던져지므로써 대응을 강제한다.
	*/

		db.Execute(query);   // 실패해도 여기선 조용함 (bFailed만 세팅됨)
		db.Execute(query);   // ClearError() 안 했으므로 진입하자마자 예외 던짐
		
		// ## 로그 파일
		/*
			| `Error_CDB_<스레드이름 > _<날짜>.txt` | 실패할 때마다 |
			| `unhandleError.txt` | 실패 확인 안 하고 다음 호출 이어갔을 때 |
		*/

}

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
	auto DBinit = []() -> int {
			mysql_library_init(0, nullptr, nullptr);
			return 1;
		};
	static int initVal = DBinit();

	mysql_thread_init();
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
	if (mConn != nullptr)
	{
		mysql_close(mConn);
		mConn = nullptr;
	}
	mysql_thread_end();
	throwIfFailed();
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
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
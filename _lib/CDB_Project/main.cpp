#include "CDB.h"
#include <iostream>

int main()
{
    SetThreadDescription(GetCurrentThread(), L"mainThread");
    CDB db;
    if (!db.Connect("localhost", "root", "123123", "accountdb", 3306))
    {
        // 대응 
        db.ClearError();
        std::cout << "Connect 실패\n";
        return 1;
    }
    std::cout << "Connect 성공\n";

    bool ok = db.Execute("CREATE TABLE IF NOT EXISTS TestTable (id INT PRIMARY KEY, value INT)");
    std::cout << "CREATE TABLE : " << (ok ? "성공" : "실패") << "\n";

    ok = db.Execute("INSERT INTO TestTable (id, value) VALUES (1, 100) ON DUPLICATE KEY UPDATE value = 100");
    std::cout << "INSERT : " << (ok ? "성공" : "실패") << "\n";

    stResultSet rs;
    if (db.Query("SELECT id, value FROM TestTable", rs))
    {
        while (rs.Fetch())   // 호출할 때마다 다음 행으로 이동, 더 없으면 false → 루프 종료
        {
            const char* id = rs.GetValue("id");
            const char* value = rs.GetValue("value");
            std::cout << "id=" << id << " value=" << value << "\n";
        }
    }

}
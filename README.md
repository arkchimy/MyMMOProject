# MyMMOProject

실시간 필드 전투 2D MMORPG — **IOCP 네트워크 라이브러리 · 필드 서버 · 클라이언트 · 부하테스트 봇**까지 전부 직접 구현한 개인 프로젝트입니다.

**여러 유저가 한 필드에서 동시에 몬스터와 실시간 전투를 벌이는 구조**를 택했습니다.
이 한 장면 안에 IOCP 비동기 I/O, 스레드 간 무락(lock-free) 메세지 통신, **몬스터를 잡을 때마다 MySQL과 Redis 랭킹이 동시에 갱신되는 구조**까지 담겨 있습니다.

[![C++](https://img.shields.io/badge/C%2B%2B-00599C?style=flat&logo=c%2B%2B&logoColor=white)](#)
[![Windows IOCP](https://img.shields.io/badge/Windows-IOCP-0078D6?style=flat&logo=windows&logoColor=white)](#)
[![MySQL](https://img.shields.io/badge/MySQL-8.x-4479A1?style=flat&logo=mysql&logoColor=white)](#)
[![Redis](https://img.shields.io/badge/Redis-cpp__redis-DC382D?style=flat&logo=redis&logoColor=white)](#)
[![DirectX11](https://img.shields.io/badge/DirectX-11-107C10?style=flat&logo=directx&logoColor=white)](#)

> **설계 배경, 트러블슈팅 과정, 코드 상세 설명**은 포트폴리오에서 확인하실 수 있습니다.

---

## 한눈에 보기

| 항목 | 내용 |
|---|---|
| 기간 | 2026.06.11 ~ 2026.08.18 |
| 인원 | 1인 개발 (서버 · 클라이언트 · 부하테스트 봇 · 네트워크 라이브러리 전부 직접 구현) |
| 핵심 기술 | IOCP 비동기 I/O, 스레드 간 무락(lock-free) 통신, MySQL/Redis 이중 쓰기, 섹터 기반 AOI |
| 기술 스택 | C++, Windows IOCP, MySQL, Redis(cpp_redis), DirectX11 |

---

## 아키텍처

```
[클라이언트] ── TCP
                 │
        IOCP WorkerThread × 2   (accept/recv/send 완료 처리)
                 │
        ┌────────▼───────────────────────────────┐
        │     FieldServer                        │
        │        ├─ authThread ── MySQL          │
        │        └─ fieldThread × N              │
        │            └─ dbThread(MySQL/Redis)    │
        │       (필드당 1:1, 각자 전용 커넥션)     │
        └────────────────────────────────────────┘
```

- 스케일아웃 전까지는 별도 LoginServer가 불필요하다고 판단했습니다 (판단 근거는 [PORTFOLIO.md](./PORTFOLIO.md) 참고).
- MySQL 블로킹 호출이 게임 로직의 20ms 틱을 막지 않도록 인증 / 게임 로직 / DB를 각각 별도 스레드로 분리했습니다.
- 이동 · 전투는 섹터 기반 AOI로 주변 플레이어에게만 브로드캐스트하고, 랭킹은 Redis로 조회합니다.

---

## 핵심 하이라이트

- **스레드 간 무락(lock-free) 통신** — 필드마다 dbThread를 1:1로 분리해 SPSC 큐로 락을 제거했습니다.
- **MySQL + Redis 실시간 이중 쓰기** — 몬스터 처치 시 킬카운트를 두 저장소에 동시 갱신하고, 랭킹 TOP 200은 Redis ZSET 조회 한 번으로 처리합니다.
- **섹터 기반 AOI 브로드캐스트** — 주변 유저에게만 이동/전투 패킷을 전송해 불필요한 트래픽을 줄였습니다.
- **Attack-Stun 레이스 디버깅** — 클라이언트/서버 상태 지속시간 차이로 패킷이 조용히 버려지던 문제를 타임라인 추적으로 원인 규명·수정했습니다.

---

## 기술 스택

C++ · Windows IOCP · MySQL · Redis (cpp_redis) · DirectX11 (클라이언트)

---

## 폴더 구조

```
FieldServer/            인증 + 필드 전투 서버 (IOCP, MySQL/Redis)
ClientProject/          DirectX11 클라이언트
ClientBotProject/       부하테스트용 봇 클라이언트
_lib/
  AcceptEx_IOCP_NetworkLib/  직접 구현한 IOCP 비동기 네트워크 라이브러리
  CDB/                       MySQL 연동 래퍼 클래스
  CrushDump_lib/             크래시 덤프 수집 유틸
  MTProfiler_Lib/            멀티스레드 프로파일러
_Shared/
  Protocol.h                 서버-클라 공유 패킷 프로토콜
```

---

## 실행 방법

**사전 준비**: Visual Studio 2022, MySQL 8.x, Redis

1. MySQL에 DB를 생성하고 `Accounts` 테이블을 만듭니다 (코드에서 쓰는 컬럼 기준):

   | 컬럼 | 타입 | 용도 |
   |---|---|---|
   | accountNo | BIGINT, PK, AUTO_INCREMENT | 계정 식별자 |
   | id | VARCHAR(19), UNIQUE | 로그인 ID (닉네임으로 재사용) |
   | pw | VARCHAR(64) | 비밀번호 (평문 저장) |
   | x, y | FLOAT | 마지막 저장 위치 |
   | killCount | BIGINT, DEFAULT 0 | 몬스터 처치 수 (Redis 이중 갱신) |
   | lastKillTime | DATETIME | 킬카운트 동률 시 보조 정렬용 |

   ```sql
   CREATE TABLE Accounts (
       accountNo    BIGINT AUTO_INCREMENT PRIMARY KEY,
       id           VARCHAR(19) NOT NULL UNIQUE,
       pw           VARCHAR(64) NOT NULL,
       x            FLOAT NOT NULL DEFAULT 0,
       y            FLOAT NOT NULL DEFAULT 0,
       killCount    BIGINT NOT NULL DEFAULT 0,
       lastKillTime DATETIME NULL
   );
   ```

2. Redis를 로컬에서 실행합니다 (기본 `127.0.0.1:6379`).
3. `FieldServer/FieldServer.slnx`를 빌드 후 실행합니다 — DB 접속 정보는 `FieldServer.cpp` 상단에서 직접 수정합니다.
4. `ClientProject/ClientProject.slnx`를 빌드 후 실행합니다 — 서버 주소/포트는 `ClientProject/Network/NetConfig.h`에서 설정합니다 (기본 포트 32000).

---
더 깊은 설계 의도와 트러블슈팅 과정이 궁금하시다면 포트폴리오를 참고해 주세요.

# MyMMOProject

실시간 필드 전투 2D MMORPG — **IOCP 네트워크 라이브러리 · 필드 서버 · 클라이언트 · 부하테스트 봇**까지 전부 직접 구현한 개인 프로젝트입니다.

**여러 유저가 한 필드에서 동시에 몬스터와 실시간 전투를 벌이는 구조**를 택했습니다.
이 한 장면 안에 IOCP 비동기 I/O, 스레드 간 무락(lock-free) 메세지 통신, **몬스터를 잡을 때마다 MySQL과 Redis 랭킹이 동시에 갱신되는 구조**까지 담겨 있습니다.

[![C++](https://img.shields.io/badge/C%2B%2B-00599C?style=flat&logo=c%2B%2B&logoColor=white)](#)
[![Windows IOCP](https://img.shields.io/badge/Windows-IOCP-0078D6?style=flat&logo=windows&logoColor=white)](#)
[![MySQL](https://img.shields.io/badge/MySQL-8.x-4479A1?style=flat&logo=mysql&logoColor=white)](#)
[![Redis](https://img.shields.io/badge/Redis-cpp__redis-DC382D?style=flat&logo=redis&logoColor=white)](#)
[![DirectX11](https://img.shields.io/badge/DirectX-11-107C10?style=flat&logo=directx&logoColor=white)](#)

> **설계 배경, 트러블슈팅 과정, 코드 상세 설명**은 포트폴리오에 담았습니다.

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
├─ FieldServer/       인증 + 필드 전투 서버 (IOCP, MySQL/Redis)
├─ ClientProject/     DirectX11 클라이언트
├─ ClientBotProject/  부하테스트용 봇 클라이언트
├─ _lib/              직접 구현한 공용 라이브러리
│  ├─ AcceptEx_IOCP_NetworkLib/  IOCP 비동기 네트워크 라이브러리
│  ├─ CDB/                       MySQL 연동 래퍼 클래스
│  ├─ CrushDump_lib/             크래시 덤프 수집 유틸
│  └─ MTProfiler_Lib/            멀티스레드 프로파일러
└─ _Shared/
   └─ Protocol.h     서버-클라 공유 패킷 프로토콜
```


---

## 실행 방법

**사전 준비**: Visual Studio 2022, MySQL 8.x, Redis

1. Redis를 로컬에서 실행합니다 (기본 `127.0.0.1:6379`).
2. `FieldServer/FieldServer.slnx`를 빌드 후 실행합니다 — DB 접속 정보는 `FieldServer.cpp` 상단에서 직접 수정합니다.
3. `ClientProject/ClientProject.slnx`를 빌드 후 실행합니다 — 서버 주소/포트는 `ClientProject/Network/NetConfig.h`에서 설정.
4. 'ClientBotProject'를 빌드 후 실행합니다.
---
  ## 설계 버전과 다음 단계

  현재 구현은 v1(단일 인스턴스) 기준이며, 트래픽 규모가 커지는 시점을 기준으로 v2 분리를 계획하고 있습니다.

  | 구분 | v1 (현재 구현) | v2 (분리 계획) |
  |---|---|---|
  | 인증 | FieldServer 내 authThread에서 처리 | 별도 LoginServer 프로세스로 분리 |
  | 랭킹 | Redis ZSET 단일 인스턴스, FieldServer가 직접 갱신 | 랭킹 집계 서버 분리, FieldServer는 이벤트만 발행 |

  **v2로 갈 때 바뀌는 것**
  - LoginServer가 세션 토큰을 발급하고 FieldServer는 토큰만 검증 (인증 로직 중복 제거)
  - 랭킹 갱신은 FieldServer → 메시지 큐 → 랭킹 집계 서버로 분리

  설계 판단의 세부 근거(병목 지점 추정, 장애 격리 시나리오)는 포트폴리오에 정리했습니다.

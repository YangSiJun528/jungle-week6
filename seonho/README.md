# Cycle Compact

간단한 C 기반 SQL 처리기입니다.

현재 구현 범위는 아래 3가지만 지원합니다.

- `insert into 테이블 values (...)`
- `select * from 테이블`
- `.exit`

프로그램 흐름은 아래와 같습니다.

`입력 -> parse() -> execute() -> 테이블이름.csv 저장 또는 출력`

## 파일 설명

- `cycle_compact/main.c`
  - REPL, `parse`, `execute`가 들어 있는 메인 코드
- `cycle_compact/test.c`
  - `assert` 기반 테스트 코드
- `cycle_compact/input.sql`
  - 파일 입력 시연용 샘플 SQL
- `cycle_compact/<table>.csv`
  - 실행 후 생성되는 실제 저장 파일
- `cycle_compact/data.csv`
  - 이전 단계에서 쓰던 파일로, 현재 구조에서는 핵심 파일이 아님

## 지원 문법

### 1. insert

```sql
insert into usertbl values ('덕배', 2, 'db2', '010-1234-5678', '감기')
```

위 문장은 `usertbl.csv` 파일에 아래처럼 저장됩니다.

```csv
덕배,2,db2,010-1234-5678,감기
```

### 2. select

```sql
select * from usertbl
```

위 문장은 `usertbl.csv` 파일의 내용을 그대로 출력합니다.

### 3. 종료

```text
.exit
```

## 실행 방법

아래 명령은 PowerShell 기준입니다.

먼저 작업 폴더로 이동합니다.

```powershell
cd .\seonho\cycle_compact
```

### 이미 `main.exe`가 있는 경우

```powershell
.\main.exe
```

### 직접 컴파일하는 경우

GCC 사용 시:

```powershell
gcc .\main.c -o .\main.exe
```

MSVC(`cl`) 사용 시:

```powershell
cl /Fe:.\main.exe .\main.c
```

컴파일 후 실행:

```powershell
.\main.exe
```

## REPL 시연 방법

아래 순서로 시연하면 됩니다.

### 1. 기존 결과 파일 삭제

```powershell
Remove-Item .\usertbl.csv -ErrorAction SilentlyContinue
```

### 2. 프로그램 실행

```powershell
.\main.exe
```

### 3. 프로그램 안에서 아래 입력

```text
insert into usertbl values ('덕배', 2, 'db2', '010-1234-5678', '감기')
insert into usertbl values ('민수', 3, 'db3', '010-0000-0000', '정상')
select * from usertbl
.exit
```

### 4. 기대되는 출력 예시

```text
db > saved
db > saved
db > 덕배,2,db2,010-1234-5678,감기
민수,3,db3,010-0000-0000,정상
db >
```

### 5. 생성된 CSV 확인

```powershell
Get-Content .\usertbl.csv
```

## 파일 입력 시연 방법

`input.sql` 파일을 한 번에 넣어서 실행할 수 있습니다.

```powershell
Get-Content .\input.sql | .\main.exe
```

즉, `input.sql`은 프로그램이 직접 여는 파일이 아니라, 표준입력으로 넣어 주는 시연용 파일입니다.

## 테스트 실행 방법

### 1. 테스트 컴파일

GCC 사용 시:

```powershell
gcc .\test.c -o .\test.exe
```

MSVC(`cl`) 사용 시:

```powershell
cl /Fe:.\test.exe .\test.c
```

### 2. 테스트 실행

```powershell
.\test.exe
```

### 3. 테스트 성공 기준

- 아무 출력 없이 종료되면 성공
- 중간에 멈추거나 assert 오류가 나면 실패

## 주의

- 현재 프로그램은 매우 단순한 과제용 구조입니다.
- `select * from 테이블`만 지원합니다.
- `insert into 테이블 values (...)` 형식만 지원합니다.
- 컬럼 이름을 따로 해석하지 않고, `values(...)` 안 문자열을 CSV 한 줄로 저장합니다.
- 테이블마다 `테이블이름.csv` 파일이 따로 생성됩니다.

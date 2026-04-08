# SQL `execute` 상세 구현 계획

## 작업 목표

- 이번 단계의 목표는 `parse` 다음 단계인 `execute`를 구현하는 것이다.
- `execute`는 파싱이 끝난 `Statement`를 받아 실제 동작을 수행한다.
- 현재 과제 범위에서 `execute`가 해야 하는 일은 아래 두 가지다.
  - `INSERT` 문장을 파일에 저장한다.
  - `SELECT` 문장을 파일에서 읽어 출력한다.
- 이번 단계는 `입력 -> 파싱 -> 실행 -> 저장` 흐름 중 마지막 빈 칸을 채우는 작업이다.

## 기준 문서

- `AGENTS.md`
  - 테스트는 반드시 작성해야 한다.
  - 기능 코드는 `main.c`에만 작성해야 한다.
  - 단순하고 이해하기 쉬운 구조를 우선한다.
  - 수정은 `haegeon/` 디렉토리 내부에서만 수행해야 한다.
- `docs/GOAL.md`
  - 최소 지원 SQL은 `INSERT`, `SELECT`다.
  - `INSERT`는 파일에 데이터를 저장해야 한다.
  - `SELECT`는 파일에서 데이터를 읽어 출력해야 한다.
  - `schema` 및 `table`은 이미 존재한다고 가정한다.

## 현재 상태 분석

### 1. `parse.h`

- 현재 시스템의 핵심 데이터 계약이 이미 정의되어 있다.
- `Statement`는 아래 정보만 가진다.
  - 문장 종류
  - 테이블 이름
  - 값 개수
  - 값 배열
- 컬럼 정보는 전혀 없다.
- 따라서 현재 `execute`는 컬럼 기반 실행기가 아니라, 행 전체를 저장하고 다시 읽는 형태로 설계해야 한다.

### 2. `parse.c`

- 파서는 이미 요구 범위 안에서 충분히 구현되어 있다.
- `SELECT * FROM table;`
- `INSERT INTO table VALUES (...);`
- 파서는 실행 책임을 전혀 가지지 않는다.
- 파일 존재 여부, 테이블 존재 여부, 저장 포맷, 출력 포맷은 모두 아직 미구현 상태다.

### 3. `main.c`

- 현재 `main.c`는 `stdin`에서 한 줄씩 SQL을 읽는다.
- `.exit`, `.quit`은 종료 명령으로 처리한다.
- `parse` 성공 시 파싱 요약만 출력한다.
- 실제 실행 구간은 아래 placeholder에서 멈춘다.

```c
printf("[executor placeholder] executor should run the statement here.\n");
```

- 즉, 현재 프로그램은 "문장을 해석할 수는 있지만 실행은 못하는 상태"다.

### 4. `test_parse.c`

- 파서 단위 테스트는 이미 잘 갖춰져 있다.
- 성공 케이스, 실패 케이스, 경계 케이스가 포함되어 있다.
- 테스트 스타일은 이후 `execute` 테스트가 그대로 따라야 할 기준점이다.

### 5. `docs/test-strategy-notes.md`

- `parse -> 단위 테스트`
- `execute -> 단위 테스트`
- `main -> 통합 테스트`
- 문서에서는 `main()`을 얇게 유지하고 `run_cli()` 같은 함수로 흐름을 분리하는 방향을 권장한다.
- 이는 `execute` 구현 후 테스트 가능성을 확보하는 데 매우 중요하다.

### 6. `docs/sql-input-spec-notes.md`

- 입력은 기본적으로 `stdin` 기반으로 처리하는 방향이 권장되어 있다.
- `./sql_processor < input.txt` 같은 실행 방식이 명세와 가장 잘 맞는다.
- 현재 `main.c`의 입력 방식은 이 방향과 대체로 맞는다.
- 다만 현재는 "한 줄 = 한 문장" 전제이고, 문서에서는 장기적으로 `;` 기준 문장 누적도 고려하고 있다.

### 7. 현재 구조에서 중요한 제약

- `AGENTS.md`는 기능 코드를 `main.c`에만 작성하라고 요구한다.
- 그러나 현재 파서 코드는 이미 `parse.c`, `parse.h`로 분리되어 있다.
- 즉, 기존 구조와 작업 규칙 사이에 이미 약한 충돌이 있다.
- 이번 단계에서는 규칙을 최대한 존중해 `execute` 관련 기능 코드는 새 `execute.c`를 만들기보다 `main.c` 안에 두는 편이 가장 안전하다.

## 이번 단계에서 해결해야 하는 문제

### 1. 실행 인터페이스 부재

- 현재는 `Statement`를 실제 동작으로 바꾸는 함수가 없다.
- 따라서 `parse` 성공 결과를 받아도 아무 일도 일어나지 않는다.

### 2. 저장 포맷 부재

- `INSERT` 데이터를 어떤 형식으로 파일에 저장할지 아직 정해지지 않았다.
- 컬럼 정보가 없기 때문에 현재는 "행 전체를 한 줄로 저장"하는 방식이 가장 자연스럽다.

### 3. 테이블 파일 경로 규칙 부재

- `users` 테이블이 실제로 어떤 파일에 대응되는지 정해져 있지 않다.
- `execute`는 `table_name`을 파일 경로로 매핑하는 규칙이 필요하다.

### 4. 실행 결과 계약 부재

- 성공/실패를 어떤 값으로 반환할지 정해져 있지 않다.
- `parse`처럼 에러 코드를 세분화하면 테스트와 디버깅이 쉬워진다.

### 5. 테스트 대상 경계 부재

- `execute`를 테스트하려면 입력 파일, 출력 스트림, 에러 스트림, 테이블 파일 상태를 제어할 수 있어야 한다.
- 현재 `main.c`는 `stdin`, `stdout`, `stderr`에 직접 묶여 있어 테스트에 불리하다.

## 설계 원칙

- `parse`와 마찬가지로 `execute`도 단순한 C 함수로 구현한다.
- 동적 메모리 할당은 가능하면 사용하지 않는다.
- 고정 길이 버퍼와 명시적 에러 코드를 사용한다.
- 저장 포맷은 사람이 읽을 수 있고 디버깅이 쉬운 텍스트 포맷으로 시작한다.
- 테이블 파일이 없으면 자동 생성하지 않고 실패로 처리하는 방향을 우선 고려한다.
  - 이유: `GOAL.md`에서 `schema`와 `table`이 이미 존재한다고 가정했기 때문이다.
- 구현은 확장성보다 현재 과제를 통과하는 단순한 흐름을 우선한다.

## 추천 인터페이스

`parse`가 "반환값은 상태 코드, 결과는 out 파라미터" 패턴을 쓰고 있으므로 `execute`도 같은 방식이 가장 단순하다.

```c
int execute(const Statement *statement,
            const char *db_dir,
            FILE *out,
            ExecuteResponse *out_response);
```

### 인자 의미

- `statement`
  - `parse`가 만든 실행 대상 문장이다.
- `db_dir`
  - 테이블 파일들이 들어 있는 기본 디렉토리다.
  - 테스트에서는 임시 디렉토리를 넣을 수 있다.
- `out`
  - `SELECT` 결과를 기록할 출력 스트림이다.
- `out_response`
  - 실행 결과 요약을 전달할 구조체다.

### 반환값 의미

- 성공 시
  - `EXECUTE_OK`
- 실패 시
  - `EXECUTE_ERROR_*`

## 내부 자료구조 계획

### 1. 실행 결과 코드

```c
typedef enum {
    EXECUTE_OK = 0,
    EXECUTE_ERROR_NULL_STATEMENT,
    EXECUTE_ERROR_NULL_DB_DIR,
    EXECUTE_ERROR_NULL_OUTPUT,
    EXECUTE_ERROR_NULL_RESPONSE,
    EXECUTE_ERROR_UNSUPPORTED_STATEMENT,
    EXECUTE_ERROR_PATH_TOO_LONG,
    EXECUTE_ERROR_TABLE_NOT_FOUND,
    EXECUTE_ERROR_FILE_OPEN_FAILED,
    EXECUTE_ERROR_FILE_READ_FAILED,
    EXECUTE_ERROR_FILE_WRITE_FAILED,
    EXECUTE_ERROR_ROW_FORMAT_TOO_LONG
} ExecuteResult;
```

### 2. 실행 응답 구조체

```c
typedef struct {
    int rows_affected;
    int rows_returned;
} ExecuteResponse;
```

### 구조체를 두는 이유

- `INSERT`와 `SELECT`의 성공 결과가 서로 다르다.
- 단순 성공/실패 외에 영향을 준 행 수를 전달하면 테스트가 쉬워진다.
- `main`이 사용자 메시지를 구성할 때도 활용할 수 있다.

## 테이블 파일 경로 계획

### 경로 규칙

- 기본 규칙은 아래처럼 단순하게 잡는다.

```text
<db_dir>/<table_name>.table
```

### 예시

- `db_dir = "haegeon/data"`
- `table_name = "users"`
- 실제 파일 경로 = `"haegeon/data/users.table"`

### 이렇게 정하는 이유

- 테이블 이름과 파일 이름의 대응이 직관적이다.
- 확장자 `.table`은 역할이 명확하다.
- 테스트에서 테이블 파일을 미리 만들고 제어하기 쉽다.

## 데이터 저장 포맷 계획

### 기본 원칙

- 각 테이블 파일은 "행 여러 개가 들어 있는 텍스트 파일"로 본다.
- 한 줄이 한 행이다.
- `INSERT`는 한 줄을 append 한다.
- `SELECT`는 모든 줄을 순서대로 읽어 출력한다.

### 한 행 직렬화 방식

- 문자열 값은 작은따옴표를 다시 붙여 저장한다.
- 숫자 값은 그대로 저장한다.
- 값은 `, `로 구분한다.
- 행 전체는 괄호로 감싼다.

### 예시

입력 SQL:

```sql
INSERT INTO users VALUES ('kim', 1, 'db2');
```

파일에 저장되는 한 줄:

```text
('kim', 1, 'db2')
```

### 이 포맷을 선택하는 이유

- 사람이 읽기 쉽다.
- 디버깅이 쉽다.
- 현재 `Statement.values[]` 구조만으로 곧바로 만들 수 있다.
- 컬럼 정보가 없는 상태에서도 무리 없이 동작한다.

### 현재 단계에서 의도적으로 하지 않는 것

- CSV escaping
- binary 포맷
- 컬럼 이름 저장
- 타입 메타데이터 저장
- schema 기반 검증

## `INSERT` 실행 계획

### 흐름

1. `statement == NULL`인지 검사한다.
2. `db_dir == NULL`인지 검사한다.
3. `out_response == NULL`인지 검사한다.
4. `statement->type`이 `STATEMENT_INSERT`인지 확인한다.
5. 테이블 파일 경로를 생성한다.
6. 테이블 파일이 존재하는지 확인한다.
7. append 모드로 파일을 연다.
8. `Statement.values[]`를 행 문자열로 직렬화한다.
9. 직렬화 결과를 파일에 쓴다.
10. 마지막에 줄바꿈 `\n`을 쓴다.
11. flush/close를 수행한다.
12. 성공 시 `rows_affected = 1`, `rows_returned = 0`으로 채운다.

### 실패 처리

- 경로 생성 버퍼가 부족하면 `EXECUTE_ERROR_PATH_TOO_LONG`
- 테이블 파일이 없으면 `EXECUTE_ERROR_TABLE_NOT_FOUND`
- 파일 열기에 실패하면 `EXECUTE_ERROR_FILE_OPEN_FAILED`
- 쓰기 실패 시 `EXECUTE_ERROR_FILE_WRITE_FAILED`
- 한 행 문자열이 내부 버퍼보다 길면 `EXECUTE_ERROR_ROW_FORMAT_TOO_LONG`

## `SELECT` 실행 계획

### 흐름

1. `statement == NULL`인지 검사한다.
2. `db_dir == NULL`인지 검사한다.
3. `out == NULL`인지 검사한다.
4. `out_response == NULL`인지 검사한다.
5. `statement->type`이 `STATEMENT_SELECT`인지 확인한다.
6. 테이블 파일 경로를 생성한다.
7. 읽기 모드로 파일을 연다.
8. 파일의 각 줄을 순서대로 읽는다.
9. 읽은 줄을 그대로 `out`에 출력한다.
10. 출력한 줄 수를 센다.
11. 파일을 닫는다.
12. 성공 시 `rows_returned`에 줄 수를 기록한다.

### 출력 정책

- 현재 `SELECT *`는 컬럼을 해석하지 못하므로 저장된 행 문자열을 그대로 출력한다.
- 즉, 출력은 아래 같은 형태가 된다.

```text
('kim', 1)
('lee', 2)
```

### 빈 파일 처리

- 테이블 파일은 존재하지만 내용이 없을 수 있다.
- 이 경우는 오류가 아니라 "0행 조회 성공"으로 처리하는 편이 자연스럽다.

## 보조 함수 계획

`AGENTS.md`를 존중해 기능 코드는 `main.c` 내부의 `static` 함수로 작성한다.

### 추천 보조 함수

- `static int build_table_path(const char *db_dir, const char *table_name, char *buffer, size_t buffer_size);`
- `static int serialize_insert_row(const Statement *statement, char *buffer, size_t buffer_size);`
- `static int execute_insert(const Statement *statement, const char *db_dir, ExecuteResponse *out_response);`
- `static int execute_select(const Statement *statement, const char *db_dir, FILE *out, ExecuteResponse *out_response);`
- `static int run_cli(FILE *in, FILE *out, FILE *err, const char *db_dir);`

### 분해 기준

- 함수 수는 과하게 늘리지 않는다.
- 핵심 흐름이 한눈에 보이는 수준으로만 분리한다.
- 외부에 공개할 필요가 없는 함수는 모두 `static`으로 둔다.

## `main` 리팩터링 계획

### 현재 문제

- 현재 `main()`은 직접 `stdin`, `stdout`, `stderr`를 다룬다.
- 이 구조는 통합 테스트를 쓰기 어렵다.

### 권장 구조

```c
int run_cli(FILE *in, FILE *out, FILE *err, const char *db_dir);

int main(void) {
    return run_cli(stdin, stdout, stderr, "haegeon/data");
}
```

### 장점

- 입출력 스트림을 테스트에서 교체할 수 있다.
- `parse -> execute -> 출력` 흐름을 실제로 검증할 수 있다.
- `main()`이 얇아져 역할이 분명해진다.

## 테스트 계획

`AGENTS.md`에 따라 테스트는 반드시 추가한다.

### 1. `execute` 단위 테스트

#### 성공 테스트

- `execute_insert_appends_row_to_existing_table`
  - 기존 테이블 파일이 있을 때 `INSERT`는 행을 하나 추가해야 한다.
- `execute_insert_preserves_string_and_integer_format`
  - 문자열과 숫자가 저장 포맷 규칙대로 기록되어야 한다.
- `execute_select_prints_all_rows`
  - `SELECT`는 파일의 모든 줄을 출력해야 한다.
- `execute_select_returns_zero_rows_for_empty_table`
  - 빈 테이블 파일은 0행 성공으로 처리해야 한다.

#### 실패 테스트

- `execute_fails_when_statement_is_null`
- `execute_fails_when_db_dir_is_null`
- `execute_fails_when_output_is_null_for_select`
- `execute_fails_when_response_is_null`
- `execute_fails_when_table_file_does_not_exist`
- `execute_fails_when_statement_type_is_invalid`

### 2. `run_cli` 통합 테스트

- `run_cli_insert_then_select_round_trips_data`
  - `INSERT` 후 `SELECT` 하면 저장된 값이 출력되어야 한다.
- `run_cli_continues_after_parse_error`
  - 잘못된 SQL이 있어도 다음 문장은 계속 처리해야 한다.
- `run_cli_writes_parse_error_to_err_stream`
  - 파싱 실패 시 에러 메시지가 `err`로 가야 한다.
- `run_cli_writes_execute_error_to_err_stream`
  - 실행 실패 시 에러 메시지가 `err`로 가야 한다.

### 3. 테스트 구현 방식

- `test_parse.c` 스타일을 그대로 따른다.
- 테스트 함수 이름은 영어로 작성한다.
- 본문은 `given`, `when`, `then` 주석으로 구분한다.
- 임시 디렉토리와 임시 파일을 사용해 파일 상태를 제어한다.
- `main()`과 테스트 엔트리포인트 충돌을 피하려면 조건부 컴파일이 필요할 수 있다.

## 구현 순서 제안

### 1단계. 실행 타입 정의

- `ExecuteResult`
- `ExecuteResponse`
- 경로 버퍼 크기, 행 버퍼 크기 상수

### 2단계. `main.c` 리팩터링

- `run_cli()`를 도입한다.
- 기존 `main()`의 입력 루프를 `run_cli()`로 옮긴다.
- `main()`은 실제 연결만 담당하도록 얇게 만든다.

### 3단계. 경로 생성 보조 함수 구현

- `db_dir + table_name + extension` 조합
- 길이 초과 시 실패 처리

### 4단계. 행 직렬화 구현

- `INSERT` 값을 텍스트 한 줄로 만든다.
- 문자열/정수 타입별 출력 규칙을 적용한다.

### 5단계. `execute_insert` 구현

- 테이블 파일 존재 확인
- append 저장
- 결과 코드 반환

### 6단계. `execute_select` 구현

- 파일 읽기
- 출력 스트림 기록
- 반환 행 수 계산

### 7단계. `execute` dispatcher 구현

- 문장 타입에 따라 `INSERT`, `SELECT` 경로로 분기한다.

### 8단계. 단위 테스트 작성

- 성공/실패/경계 케이스를 모두 추가한다.

### 9단계. 통합 테스트 작성

- `parse -> execute -> 출력` 전체 흐름을 검증한다.

## 이번 단계의 비범위

- 컬럼 단위 `SELECT`
- `WHERE`
- `UPDATE`, `DELETE`
- schema 검증
- 컬럼 개수 검증
- 타입 변환 검증
- quoted identifier
- SQL escape 처리
- 여러 줄 SQL 누적 입력 최적화
- 파일이 없을 때 테이블 자동 생성

## 최종 판단

가장 단순하고 안전한 구현 방향은 다음과 같다.

- `execute`는 `parse`와 같은 패턴으로 상태 코드를 반환한다.
- 실행 관련 기능 코드는 `AGENTS.md`를 존중해 우선 `main.c`에 둔다.
- 저장 포맷은 "한 줄 = 한 행" 텍스트 포맷으로 시작한다.
- `INSERT`는 기존 테이블 파일에 append 한다.
- `SELECT`는 테이블 파일을 읽어 줄 단위로 출력한다.
- `run_cli()`를 도입해 통합 테스트 가능한 구조로 정리한다.

이 계획대로 구현하면 현재 과제의 요구사항을 직접 충족하면서도, 코드와 테스트 모두 설명하기 쉬운 형태를 유지할 수 있다.

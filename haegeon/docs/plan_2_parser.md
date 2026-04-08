# SQL `parse` 상세 구현 계획

## 작업 목표

- `parse`는 SQL 문자열 1개를 받아 내부 `Statement` 구조체로 해석한다.
- 이 모듈은 tokenizer, lexer, parser 역할을 한 번에 수행한다.
- 현재 지원 범위는 아래 두 가지 문장만으로 제한한다.
  - `select * from <table_name>;`
  - `insert into <table_name> values (<value> [, <value>]...);`
- 표기 의미
  - `<table_name>`은 실제 테이블 이름이 들어가는 자리표시자다.
  - `<value>`는 실제 값 하나가 들어가는 자리표시자다.
  - `...`는 같은 패턴이 반복될 수 있음을 뜻한다.
  - 따라서 `INSERT`의 값 목록은 1개 이상의 값을 허용한다.
- 문법 오류가 있으면 세부 오류 코드를 반환한다.
- 실행 책임은 포함하지 않는다.
  - 테이블 존재 여부 확인
  - 파일 읽기/쓰기
  - schema 일치 여부 확인

## 설계 원칙

- 파서 관련 기능 코드는 별도 파일에 둔다.
  - 예: `parse.c`, `parse.h`
  - 내부 보조 함수는 `parse.c` 안의 `static` 함수로 제한한다.
  - 실제 구현 전에는 현재 작업 규칙과 파일 분리 방침의 충돌 여부를 한 번 더 확인한다.
- 파서는 최대한 단순한 문자 스캐너로 구현한다.
- 동적 메모리 할당은 사용하지 않는다.
- 고정 길이 버퍼를 사용하고, 길이 초과는 파싱 오류로 처리한다.
- 입력 문자열은 수정하지 않는다.
- 성공 시에만 결과 구조체를 채우고, 실패 시에는 부분 결과를 외부로 노출하지 않는다.

## 함수 인터페이스 결정

요구사항 문구만 보면 `parse`가 해석된 문장을 직접 반환하는 것처럼 보이지만, C에서는 성공 결과와 실패 오류 코드를 함께 다루기 위해 아래 형태가 가장 단순하다.

```c
int parse(const char *sql, Statement *out_statement);
```

### 이렇게 정하는 이유

- 직접 반환값은 성공/실패를 나타내는 에러 코드로 쓰는 편이 테스트하기 쉽다.
- 해석 결과는 `out_statement`로 전달하면 구조체 전체를 깔끔하게 채울 수 있다.
- 이후 `main -> parse -> execute` 흐름에 연결하기 쉽다.
- 실패 시 어떤 이유로 실패했는지 세분화해 전달할 수 있다.
- 입력 문자열을 수정하지 않는다는 설계 원칙과도 맞는다.

### 동작 규칙

- 성공 시
  - 반환값은 `PARSE_OK`
  - `out_statement`에 해석 결과를 저장한다.
- 실패 시
  - 반환값은 `PARSE_ERROR_*`
  - `out_statement`는 사용하지 않는다.

## 내부 자료구조 계획

확장성보다 단순함이 중요하므로, 현재 필요한 정보만 담는 구조체로 시작한다.

```c
typedef enum {
    STATEMENT_SELECT,
    STATEMENT_INSERT
} StatementType;

typedef enum {
    VALUE_STRING,
    VALUE_INTEGER
} ValueType;

typedef struct {
    ValueType type;
    char text[256];
} ParsedValue;

typedef struct {
    StatementType type;
    char table_name[64];
    int value_count;
    ParsedValue values[16];
} Statement;
```

### 구조 설명

- `Statement.type`
  - `SELECT`인지 `INSERT`인지 구분한다.
- `Statement.table_name`
  - `FROM` 또는 `INTO` 뒤에 오는 테이블 이름을 저장한다.
- `Statement.value_count`
  - `INSERT`일 때만 의미가 있다.
- `Statement.values`
  - `INSERT` 값 목록을 저장한다.
  - 숫자와 문자열만 구분한다.

### 값 저장 방식 결정

- 문자열 값은 작은따옴표를 제거한 본문만 `text`에 저장한다.
  - 예: `'덕배'` -> `"덕배"`
- 숫자 값도 숫자 자체는 문자열로 저장한다.
  - 예: `2` -> `"2"`
- 숫자를 `int`로 바로 저장하지 않는 이유
  - 지금 단계에서는 실행보다 해석이 우선이다.
  - 범위 초과, 형변환 정책 같은 부가 결정을 뒤로 미룰 수 있다.
  - 출력/저장 단계에서 그대로 사용하기 쉽다.

## 에러 코드 계획

테스트와 디버깅을 쉽게 하려고 오류는 가능한 한 구체적으로 나눈다.

```c
typedef enum {
    PARSE_OK = 0,
    PARSE_ERROR_NULL_INPUT,
    PARSE_ERROR_NULL_OUTPUT,
    PARSE_ERROR_EMPTY_INPUT,
    PARSE_ERROR_UNKNOWN_STATEMENT,
    PARSE_ERROR_MISSING_SEMICOLON,
    PARSE_ERROR_INVALID_SELECT_TARGET,
    PARSE_ERROR_EXPECTED_FROM,
    PARSE_ERROR_EXPECTED_INTO,
    PARSE_ERROR_EXPECTED_VALUES,
    PARSE_ERROR_INVALID_TABLE_NAME,
    PARSE_ERROR_EXPECTED_OPEN_PAREN,
    PARSE_ERROR_EXPECTED_CLOSE_PAREN,
    PARSE_ERROR_EMPTY_VALUE_LIST,
    PARSE_ERROR_INVALID_VALUE,
    PARSE_ERROR_UNTERMINATED_STRING,
    PARSE_ERROR_EXPECTED_COMMA_OR_CLOSE_PAREN,
    PARSE_ERROR_TOO_MANY_VALUES,
    PARSE_ERROR_IDENTIFIER_TOO_LONG,
    PARSE_ERROR_VALUE_TOO_LONG,
    PARSE_ERROR_TRAILING_TOKENS
} ParseResult;
```

### 이번 요구사항과 직접 연결되는 핵심 오류

- `select name from users;`
  - `PARSE_ERROR_INVALID_SELECT_TARGET`
- `select * from users`
  - `PARSE_ERROR_MISSING_SEMICOLON`
- `select * users;`
  - `PARSE_ERROR_EXPECTED_FROM`
- `insert users values ('a');`
  - `PARSE_ERROR_EXPECTED_INTO`
- `insert into users ('a');`
  - `PARSE_ERROR_EXPECTED_VALUES`

## 문법 규칙 상세

파서는 SQL 표준 전체를 지원하지 않고, 현재 과제 범위에 필요한 규칙만 처리한다.

### 1. 공통 규칙

- 키워드는 대소문자를 구분하지 않는다.
  - `select`, `SELECT`, `SeLeCt` 모두 허용
- 공백은 유연하게 허용한다.
  - 공백
  - 탭
  - 줄바꿈
- 문장 앞뒤의 공백은 무시한다.
- 문장은 반드시 세미콜론 `;`으로 끝나야 한다.
- 세미콜론 뒤에는 공백만 올 수 있다.
- 한 번의 `parse` 호출은 SQL 문장 하나만 처리한다.
  - `SELECT ...; INSERT ...;` 같이 두 문장이 들어오면 오류로 본다.

### 2. 테이블 이름 규칙

- 우선 단순 식별자만 허용한다.
- 허용 형태
  - 첫 글자: 영문자 또는 `_`
  - 이후 글자: 영문자, 숫자, `_`
- 허용 예시
  - `users`
  - `user_1`
  - `_temp`
- 비허용 예시
  - `user-profile`
  - `order details`
  - `'users'`

### 3. `SELECT` 문 규칙

정확히 아래 구조만 허용한다.

```sql
select * from table_name;
```

세부 규칙은 다음과 같다.

- `select` 다음에는 반드시 `*`가 와야 한다.
- `*` 다음에는 반드시 `from`이 와야 한다.
- `from` 다음에는 반드시 테이블 이름이 와야 한다.
- 테이블 이름 뒤에는 세미콜론만 올 수 있다.
- `*` 대신 컬럼 이름이 오면 오류다.

허용 예시:

```sql
select * from users;
SELECT * FROM users;
 select   *   from   users ;
```

실패 예시:

```sql
select name from users;
select * users;
select * from ;
select * from users
```

### 4. `INSERT` 문 규칙

정확히 아래 구조만 허용한다.

```sql
insert into table_name values (value1, value2, ...);
```

세부 규칙은 다음과 같다.

- `insert` 다음에는 반드시 `into`가 와야 한다.
- `into` 다음에는 반드시 테이블 이름이 와야 한다.
- 테이블 이름 다음에는 반드시 `values`가 와야 한다.
- `values` 다음에는 반드시 `(`가 와야 한다.
- 괄호 안에는 값이 1개 이상 있어야 한다.
- 각 값은 `,`로 구분한다.
- 마지막 값 뒤에는 `)`가 와야 한다.
- 닫는 괄호 뒤에는 세미콜론만 올 수 있다.

허용 예시:

```sql
insert into users values ('덕배', 2, 'db2', '010-1234-5678', '감기');
INSERT INTO users VALUES ('kim', 1);
insert into users values(1,'kim');
```

실패 예시:

```sql
insert users values ('kim');
insert into users ('kim');
insert into users values ();
insert into users values ('kim',);
insert into users values ('kim' 1);
```

## 값 파싱 규칙

### 문자열 값

- 작은따옴표 `'`로 감싼 값을 문자열로 본다.
- 문자열 내부의 내용은 닫는 작은따옴표를 만날 때까지 그대로 읽는다.
- 한글은 그대로 허용한다.
  - UTF-8 바이트를 그대로 복사하면 된다.
- 쉼표, 공백, 하이픈은 문자열 안에서 허용된다.
  - 예: `'010-1234-5678'`
- 현재 버전에서는 문자열 이스케이프는 지원하지 않는다.
  - 예: `\'`, `''` 같은 SQL 이스케이프는 미지원

### 숫자 값

- 숫자 값은 연속된 10진 숫자만 허용한다.
- 현재 단계에서는 부호, 소수점, 지수 표기법은 지원하지 않는다.
- 예: `2`, `123`, `0` 허용
- 예: `-1`, `3.14` 비허용

### 값 목록 해석 방식

예시:

```sql
insert into patients values ('덕배', 2, 'db2', '010-1234-5678', '감기');
```

해석 결과:

- table_name = `patients`
- value_count = `5`
- values[0] = 문자열 `"덕배"`
- values[1] = 숫자 `"2"`
- values[2] = 문자열 `"db2"`
- values[3] = 문자열 `"010-1234-5678"`
- values[4] = 문자열 `"감기"`

## 파싱 절차 상세

구현은 `parse()` 하나에 모든 로직을 몰아넣지 않고, 이해 가능한 최소 단위의 보조 함수로 나눈다.

- 공개 인터페이스는 `parse.h`에 선언한다.
- 실제 파싱 로직과 내부 보조 함수는 `parse.c`에 둔다.
- 외부에 노출할 필요가 없는 보조 함수는 `static`으로 제한한다.
- 다만 보조 함수 개수는 최소한으로 유지해 흐름 추적이 쉬워야 한다.

### 추천 보조 함수 목록

- `static void skip_spaces(const char **cursor);`
- `static int read_identifier(const char **cursor, char *buffer, size_t buffer_size);`
- `static int match_keyword(const char **cursor, const char *keyword);`
- `static int parse_insert_value(const char **cursor, ParsedValue *out_value);`
- `static int finish_with_semicolon(const char **cursor);`
- `static int parse_select_statement(const char **cursor, Statement *out_statement);`
- `static int parse_insert_statement(const char **cursor, Statement *out_statement);`

### 보조 함수 분해 기준

- 이번 단계에서는 helper를 과하게 잘게 나누지 않는다.
- `is_identifier_start`, `is_identifier_char`, `expect_char` 정도는 구현 중 코드가 짧다면 호출부 안에 녹여도 된다.
- `parse_string_value`, `parse_integer_value`도 `parse_insert_value` 내부가 지나치게 길어질 때만 분리한다.
- 즉, 기본 계획은 핵심 흐름이 잘 보이는 6~7개 수준의 보조 함수만 유지하는 것이다.

### `parse()` 전체 흐름

1. `sql == NULL`인지 검사한다.
2. `out_statement == NULL`인지 검사한다.
3. 임시 `Statement temp = {0};`를 만든다.
4. 커서를 입력 문자열 시작 위치에 둔다.
5. 앞쪽 공백을 건너뛴다.
6. 빈 문자열이면 `PARSE_ERROR_EMPTY_INPUT`를 반환한다.
7. 첫 키워드를 보고 `SELECT`인지 `INSERT`인지 분기한다.
8. 각 전용 파서에서 문장을 끝까지 해석한다.
9. 성공하면 마지막에만 `*out_statement = temp;`를 수행한다.
10. 결과 코드를 반환한다.

### `SELECT` 전용 파서 흐름

1. `select` 키워드를 확인한다.
2. 공백을 건너뛴다.
3. `*`인지 확인한다.
4. 아니면 `PARSE_ERROR_INVALID_SELECT_TARGET`를 반환한다.
5. 공백을 건너뛴다.
6. `from` 키워드를 확인한다.
7. 없으면 `PARSE_ERROR_EXPECTED_FROM`
8. 공백을 건너뛴다.
9. 테이블 이름을 읽는다.
10. 실패하면 `PARSE_ERROR_INVALID_TABLE_NAME`
11. 공백을 건너뛴다.
12. 세미콜론을 확인한다.
13. 없으면 `PARSE_ERROR_MISSING_SEMICOLON`
14. 세미콜론 뒤에 공백 외 문자가 있으면 `PARSE_ERROR_TRAILING_TOKENS`
15. `Statement.type = STATEMENT_SELECT`를 채운다.

### `INSERT` 전용 파서 흐름

1. `insert` 키워드를 확인한다.
2. 공백을 건너뛴다.
3. `into` 키워드를 확인한다.
4. 없으면 `PARSE_ERROR_EXPECTED_INTO`
5. 공백을 건너뛴다.
6. 테이블 이름을 읽는다.
7. 실패하면 `PARSE_ERROR_INVALID_TABLE_NAME`
8. 공백을 건너뛴다.
9. `values` 키워드를 확인한다.
10. 없으면 `PARSE_ERROR_EXPECTED_VALUES`
11. 공백을 건너뛴다.
12. `(`를 확인한다.
13. 없으면 `PARSE_ERROR_EXPECTED_OPEN_PAREN`
14. 공백을 건너뛴다.
15. 바로 `)`가 나오면 `PARSE_ERROR_EMPTY_VALUE_LIST`
16. 첫 값을 읽는다.
17. 값 하나를 읽을 때마다 `value_count`를 증가시킨다.
18. 공백을 건너뛴다.
19. 다음 문자가 `,`면 다음 값을 계속 읽는다.
20. 다음 문자가 `)`면 값 목록 종료로 본다.
21. 그 외 문자는 `PARSE_ERROR_EXPECTED_COMMA_OR_CLOSE_PAREN`
22. 괄호를 닫은 뒤 공백을 건너뛴다.
23. 세미콜론을 확인한다.
24. 없으면 `PARSE_ERROR_MISSING_SEMICOLON`
25. 세미콜론 뒤에 공백 외 문자가 있으면 `PARSE_ERROR_TRAILING_TOKENS`
26. `Statement.type = STATEMENT_INSERT`를 채운다.

## 구현 세부 결정

### 1. 키워드 비교 방식

- `strcasecmp()` 같은 플랫폼 의존 함수를 피하고 직접 비교한다.
- 방법
  - 현재 위치에서 영문자 토큰을 하나 읽는다.
  - 각 문자를 소문자 또는 대문자로 정규화해 비교한다.
- 이유
  - 이식성이 좋다.
  - 과제 범위에서 충분히 단순하다.
- 주의할 점
  - 토큰 경계를 함께 확인해야 한다.
  - 예를 들어 `selectx`를 `select`로 잘못 인식하면 안 된다.
- 단점
  - 직접 비교 로직을 작성해야 하므로 코드가 조금 늘어난다.
  - 토큰 경계 처리를 실수하면 미묘한 버그가 생길 수 있다.
- 그래도 이 결정을 유지하는 근거
  - 이번 과제에서 비교할 키워드 수가 매우 적다.
  - 모두 ASCII 영문 키워드라 직접 비교 로직이 복잡해지지 않는다.
  - 표준 C만으로 동작하게 유지하기 쉬워 환경 의존성을 줄일 수 있다.

### 2. 공백 처리 방식

- `isspace()` 기반으로 처리한다.
- 따라서 아래는 모두 동일하게 취급할 수 있다.
  - 공백
  - 탭
  - 줄바꿈
- 이 방식이면 나중에 여러 줄 SQL 입력에도 대응하기 쉽다.

### 3. 길이 제한 정책

- 테이블 이름, 값 문자열, 값 개수는 상수로 제한한다.
- 제한을 넘으면 명확한 에러 코드로 실패시킨다.
- 이유
  - 동적 메모리 없이 단순하게 구현할 수 있다.
  - 버퍼 오버플로를 피할 수 있다.

### 4. 부분 성공 방지

- 외부 `out_statement`를 바로 채우지 않고, 임시 구조체에 먼저 기록한다.
- 문장 전체 검증이 끝난 뒤 한 번에 복사한다.
- 이유
  - 실패 시 반쯤 채워진 구조체가 남지 않는다.

## 테스트 계획

`AGENTS.md` 규칙에 따라 테스트는 반드시 작성해야 한다. 이 문서에서는 파서 구현 직후 작성할 테스트 범위를 미리 고정한다.

### 성공 테스트

- `parse_select_with_lowercase_keywords`
  - `select * from users;`는 성공해야 한다.
- `parse_select_with_uppercase_keywords`
  - `SELECT * FROM users;`는 성공해야 한다.
- `parse_select_with_extra_spaces`
  - 여러 공백이 있어도 성공해야 한다.
- `parse_insert_with_string_and_integer_values`
  - 문자열과 숫자가 섞인 `INSERT`를 성공적으로 해석해야 한다.
- `parse_insert_with_korean_strings`
  - 한글 문자열을 그대로 보존해야 한다.
- `parse_insert_without_space_after_parenthesis`
  - `values(1,'kim');`도 허용해야 한다.

### 실패 테스트

- `parse_fails_when_input_is_null`
- `parse_fails_when_output_is_null`
- `parse_fails_when_input_is_empty`
- `parse_fails_for_unknown_statement`
  - `delete from users;`
- `parse_select_fails_when_target_is_not_star`
  - `select name from users;`
- `parse_select_fails_when_from_is_missing`
  - `select * users;`
- `parse_fails_when_semicolon_is_missing`
  - `select * from users`
- `parse_insert_fails_when_into_is_missing`
  - `insert users values ('kim');`
- `parse_insert_fails_when_values_keyword_is_missing`
  - `insert into users ('kim');`
- `parse_insert_fails_when_value_list_is_empty`
  - `insert into users values ();`
- `parse_insert_fails_for_unterminated_string`
  - `insert into users values ('kim);`
- `parse_insert_fails_for_missing_comma`
  - `insert into users values ('kim' 1);`
- `parse_insert_fails_for_trailing_comma`
  - `insert into users values ('kim',);`
- `parse_fails_when_tokens_exist_after_semicolon`
  - `select * from users; extra`

### 경계 테스트

- `parse_fails_when_table_name_is_too_long`
- `parse_fails_when_value_text_is_too_long`
- `parse_fails_when_value_count_exceeds_limit`

### 테스트 검증 포인트

- 반환 코드가 기대값과 일치하는가
- 성공 시 `Statement.type`이 맞는가
- 테이블 이름이 정확히 들어갔는가
- `INSERT`의 `value_count`가 맞는가
- 각 값의 `type`과 `text`가 정확한가

## 구현 순서 제안

### 1단계. 타입과 상수 정의

- `StatementType`
- `ValueType`
- `ParsedValue`
- `Statement`
- `ParseResult`
- 길이 제한 상수

### 2단계. 공통 보조 함수 구현

- 공백 건너뛰기
- 키워드 비교
- 식별자 읽기
- 문장 종료 확인

### 3단계. 값 파서 구현

- 단일 값 파싱
  - 필요하면 내부에서 문자열/숫자 처리를 추가 분리

### 4단계. 문장별 파서 구현

- `parse_select_statement`
- `parse_insert_statement`

### 5단계. 진입점 `parse` 구현

- 입력 검사
- 문장 타입 분기
- 임시 구조체 사용
- 결과 반환

### 6단계. 단위 테스트 작성

- 성공 케이스
- 실패 케이스
- 경계 케이스

### 7단계. 이후 연결 작업

- `main()`의 placeholder를 `parse()` 호출로 교체
- 파싱 실패 시 에러 메시지 출력
- 파싱 성공 시 `execute()` 단계로 전달

## 이번 단계의 비범위

아래 항목은 지금 계획에 넣지 않는다.

- `SELECT column1, column2 ...`
- `WHERE`
- `UPDATE`, `DELETE`
- SQL 문자열 이스케이프
- quoted identifier
- 테이블 존재 여부 검증
- 컬럼 개수와 값 개수의 schema 비교
- 여러 SQL 문장을 한 번에 파싱하는 기능

## 최종 판단

가장 단순하고 안전한 구현 방향은 다음과 같다.

- `parse`는 에러 코드를 반환한다.
- 해석 결과는 `Statement` 구조체에 채운다.
- 키워드는 대소문자를 구분하지 않는다.
- `SELECT`는 오직 `*`만 허용한다.
- `INSERT`는 문자열과 숫자 값 목록만 해석한다.
- 모든 파싱은 고정 버퍼와 문자 스캐너 기반으로 처리한다.

이 계획대로 구현하면 현재 과제 요구사항을 충족하면서도, 테스트 가능하고 설명하기 쉬운 파서를 만들 수 있다.

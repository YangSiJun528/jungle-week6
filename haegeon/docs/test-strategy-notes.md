# SQL 처리기 테스트 전략 메모

## 결론

- `main()`에도 테스트를 적용할 수 있다.
- 다만 `main()` 자체를 세세한 단위 테스트 대상으로 보기보다는, 전체 흐름을 확인하는 통합 테스트 대상으로 보는 것이 더 자연스럽다.
- 테스트 대상은 다음과 같이 나누는 것이 가장 깔끔하다.

```text
parse    -> 단위 테스트
execute  -> 단위 테스트
main     -> 통합 테스트 / E2E 테스트
```

## 이유

- `main()`은 보통 입력과 출력, 전체 흐름 제어를 담당한다.
- 실제 비즈니스 로직이 `main()` 안에 많이 들어가면 테스트가 어려워진다.
- 따라서 `main()`은 얇게 유지하고, 실제 동작은 별도 함수로 분리하는 것이 좋다.

예시:

```c
int run_cli(FILE *in, FILE *out, FILE *err);

int main(void) {
    return run_cli(stdin, stdout, stderr);
}
```

- 이렇게 하면 `main()`은 연결만 담당하고,
- 실제 테스트는 `run_cli()`를 대상으로 수행할 수 있다.

## 권장 테스트 분리

### 1. parse 테스트

- `"SELECT * FROM users;"` 입력 시 SELECT 문장으로 해석되는지 확인
- `"INSERT INTO users VALUES (1, 'kim');"` 입력 시 INSERT 문장으로 해석되는지 확인
- 잘못된 SQL 입력 시 실패를 반환하는지 확인

### 2. execute 테스트

- INSERT 문장을 실행하면 실제 파일에 데이터가 추가되는지 확인
- SELECT 문장을 실행하면 파일에서 데이터를 읽어 결과를 반환하거나 출력하는지 확인
- 존재하지 않는 테이블, 잘못된 파일 상태 등에 대해 적절한 에러를 반환하는지 확인

### 3. main 또는 run_cli 테스트

- SQL 입력을 받아 `parse -> execute -> 출력` 흐름이 실제로 연결되는지 확인
- 잘못된 SQL이 들어오면 에러 메시지가 출력되는지 확인
- 여러 SQL 문장을 순서대로 처리할 수 있는지 확인
- `INSERT` 후 `SELECT` 했을 때 저장된 데이터가 정상적으로 조회되는지 확인

## main 단계에서 검증할 수 있는 것

- 입력이 들어오면 정상 결과가 출력되는가
- 잘못된 입력이 들어오면 에러가 출력되는가
- 여러 SQL 문장을 순서대로 처리하는가
- 실행 결과가 실제 파일 상태에 반영되는가

## 통합 테스트 예시

입력:

```sql
INSERT INTO users VALUES (1, 'kim');
SELECT * FROM users;
```

기대 결과:

- 프로그램 종료 코드가 0이다
- 출력 결과에 `kim`이 포함된다
- 실제 테이블 파일에 레코드가 저장된다

## 구현 권장 방향

- `stdin`, `stdout`, `stderr`에 직접 강하게 묶지 말고,
- `FILE *`를 인자로 받는 `run_cli()` 같은 함수를 만들어 테스트 가능성을 높이는 것이 좋다.

예시:

```c
int run_cli(FILE *in, FILE *out, FILE *err) {
    char buffer[1024];

    while (fgets(buffer, sizeof(buffer), in)) {
        Statement stmt;

        if (parse(buffer, &stmt) != 0) {
            fprintf(err, "parse error\n");
            continue;
        }

        if (execute(&stmt, out) != 0) {
            fprintf(err, "execute error\n");
        }
    }

    return 0;
}
```

## 최종 판단

- 팀이 정한 `UI -> 해석 -> 실행/저장` 구조에서는 `main()` 테스트가 가능하다.
- 다만 가장 좋은 방식은 `main()`을 얇게 만들고, 실제 흐름은 `run_cli()` 같은 함수로 분리해 통합 테스트하는 것이다.
- 이렇게 하면 역할 분리도 명확해지고, 팀이 정한 사이클도 코드 구조로 잘 드러난다.

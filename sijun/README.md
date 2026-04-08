# 양시준 수요코딩회 자료 모음

## 핵심 질문

### SQL의 개념

관계형 데이터베이스를 다루기 위한 고수준 질의 언어, 요즘에는 관계형 DB가 아닌 DB도 사용자의 편의를 위해 제공하기도 한다. (예: DuckDB, BigQuery, Elasticsearch)

표준화를 위해 ANSI SQL이 있지만, 실제론 각 DBMS마다 약간씩 동작이 다르다.

> 사실상, 관계형 데이터베이스에 대한 질의어 표준 (정형화된 언어)
>
> - 그러나, 단순한 데이터 질의어 그 이상의 역할을 함
>   - 즉, 검색 기능, 갱신 연산 이외에도, 데이터베이스 관리를 위한 많은 기능을 제공
>   - 데이터 구조 정의, 보안 무결성 제약조건 등도 가능
> 
> http://www.ktword.co.kr/test/view/view.php?no=282

**비 절차적** 데이터 접근 언어라는게 중요. DBMS는 점점 똑똑해지고 성능이 좋아지는데, 이게 가능한 이유가 what만 정의하는 SQL을 통해서 제어하기 때문에 내부 해석이나 실행계획 처리를 몰라도 됨. (일종의 추상화?)

(hint 등을 사용해서 행동(how)을 강제할 수도 있음.)

### ANSI SQL이란?

> ANSI SQL은 American National Standards Institute (ANSI)에서 제정한 SQL(Structured Query Language) 표준을 의미한다. ANSI는 다양한 DBMS(Database Management System)가 서로 다른 구현을 가지고 있더라도 공통적인 질의 언어(SQL)를 사용하여 데이터베이스와 상호작용할 수 있도록 표준을 개발했다.
> 
> http://www.ktword.co.kr/test/view/view.php?no=282

[Backus–Naur form](https://en.wikipedia.org/wiki/Backus%E2%80%93Naur_form) 형태로 [ANSI SQL에 대한 요구사항을 적어놓은 문서](https://ronsavage.github.io/SQL/)를 볼 수 있다. 실제로는 매우 복잡한 것을 알 수 있다.

[사람들이 ANSI 표준이라고 부르지만, 실제로 ANSI는 승인하는 쪽이지 표준을 만들고 관리하는 쪽은 아님.](https://blog.ansi.org/ansi/sql-standard-iso-iec-9075-2023-ansi-x3-135/)

각 벤더는 SQL 표준을 기준으로 기능을 제공하지만, 일부 표준 기능을 지원하지 않거나 같은 기능도 DBMS마다 세부 동작이 다를 수 있다.    
이러한 차이는 이미 각 벤더 문서에 정리되어 있으므로, 실제 사용 시에는 표준 준수 여부와 구현 차이를 확인할 필요가 있다. (예시: [PostgreSQL](https://www.postgresql.org/docs/current/features.html), [MySQL](https://dev.mysql.com/doc/refman/8.4/en/compatibility.html), [SQLite](https://sqlite.org/quirks.html))

### Database 개념

#### Database란?

> 구조적, 논리적으로 조직화된 데이터 집합
> - 여러 응용 프로그램,사용자가, 공유,활용할 수 있도록, 통합 관리되는 데이터 저장소임
> - 데이터의 정의,저장,검색,갱신 등이 DBMS을 통해 이루어짐
> 
> http://www.ktword.co.kr/test/view/view.php?m_temp1=257&id=512

#### DBMS (Database Management System)

> 데이터베이스의 생성,사용을 관리,제어하고, 질의를 처리하는 소프트웨어를 총칭
> 
> http://www.ktword.co.kr/test/view/view.php?m_temp1=259&id=513

#### RDB (Relational DataBase)

> 간결하고 수학적인 표현으로 가장 보편적으로 사용되는 데이터베이스 모델
> 
> http://www.ktword.co.kr/test/view/view.php?m_temp1=445&id=514

수학적 모델과 SQL의 관계를 보고 싶으면 "[SQL과 관계형 모델](https://kihyo-park.github.io/blog/rdb-intro/)" 글 추천

### Database의 구성 요소 

(Redis 같은 In-Memory DB도 있지만, 여기선 영속적인 저장을 위한 DB를 가정함.)

<details>
<summary>최소 구성요소에 대한 LLM의 답변</summary>
<div>

#### 1. 저장소

데이터를 디스크에 저장하고 다시 읽어오는 부분입니다.

하는 일:

* DB 파일 관리
* 페이지 단위 read/write
* 파일 포맷 유지

없으면 그냥 메모리 자료구조일 뿐이고 DB가 아닙니다.

#### 2. 쿼리 처리기

SQL을 해석해서 실제 동작으로 바꾸는 부분입니다.

하는 일:

* SQL 파싱
* 실행 계획 결정
* 실제 실행

예:

```sql
SELECT * FROM user WHERE id = 1;
```

이 문장을 보고

* user 테이블을 찾고
* 인덱스를 탈지 전체 스캔할지 정하고
* 결과를 반환합니다.

#### 3. 저장 구조

데이터를 어떤 형태로 보관하고 찾을지 결정하는 부분입니다.

하는 일:

* 테이블 구조 관리
* 인덱스 관리
* 내부 자료구조(B-tree 등) 유지

즉,

* 테이블은 데이터를 담고
* 인덱스는 빨리 찾게 해줍니다.

#### 4. 트랜잭션 / 복구

실행 중 실패해도 DB가 안 깨지게 하는 부분입니다.

하는 일:

* commit / rollback
* 일부만 반영되는 상황 방지
* 크래시 후 복구

예:
계좌 이체 중간에 프로그램이 죽어도

* 둘 다 반영되거나
* 둘 다 취소되어야 합니다.

이걸 보장하는 게 이 부분입니다.

#### 한 줄 요약

DB의 최소 핵심은 보통 이 4개입니다.

* **저장소**: 파일에 저장
* **쿼리 처리기**: SQL 해석/실행
* **저장 구조**: 테이블/인덱스 관리
* **트랜잭션/복구**: 깨지지 않게 보장

더 짧게 말하면:

**“저장하고, 해석하고, 찾고, 망가져도 복구하는 시스템”** 입니다.

</div>
</details>

## 학습 자료 보고 정리

https://cstack.github.io/db_tutorial/

이 자료를 거의 따라하다시피 할 듯?

왜냐면 각 파트의 내용이 작으면서 실행 가능한 단계로 진행됨.

Buffer Memory 같은 복잡한 서버 DBMS가 아닌, 데이터베이스의 정말 핵심만 가볍게 다루는 느낌이라 수요코딩회에 적절함.

다만 자료 자체가 SQLite 기반의 가벼운 프로젝트라 모든 DB에 적용할 수는 없을 듯 함. (따로 이론 학습이 필요. 또는 필요하다는 사실을 인지하고 있기)

### 데이터베이스 구조

SQLite에 대한 분석이 들어가는데,

컴파일러처럼 프론트엔드와 백엔드로 나뉜다.

프론트엔드는 SQL을 해석해서 실행 가능한 바이트코드를 만들고, 백엔드는 바이트코드를 실행해서 파일을 효율적으로 저장하고 가져올 수 있도록 한다.

**FE**
- tokenizer
- parser
- code generator

**BE**
- virtual machine 
- B-tree 
- pager  
- os interface

### 학습자료 보고 느낀점

확실히 좋다

- 근데 테스트 가능하게 재현 가능한 함수들, 아닌 함수들로 분리하면 좋을듯?
  - 이거는 AI한테 Agents.md로 알려주게 하고.
- C의 INPUT/OUTPUT 코드 보기 어려우니까 추상화해야겠다.
- 파일은 CSV 포맷을 하는데, 메타데이터도 테이블로 저장하고.

### 구현 계획 세우기

### 현재 parser가 구현한 EBNF

지금 구현은 아래 EBNF를 기준으로 동작한다.

```ebnf
query       = ws, (select_query | insert_query), ws, ";", ws ;

select_query = "select", req_ws, "*", req_ws, "from", req_ws, table_name ;

insert_query = "insert", req_ws, "into", req_ws, table_name,
               req_ws, "values", ws, "(", ws, value_list, ws, ")" ;

value_list  = value, { ws, ",", ws, value } ;

table_name  = identifier ;

value       = number | string ;

identifier  = letter, { letter | digit | "_" } ;

number      = digit, { digit } ;

string      = "'", { string_char }, "'" ;

string_char = letter | digit | "_" | " " ;

req_ws      = whitespace, { whitespace } ;
ws          = { whitespace } ;

whitespace  = " " | "\t" | "\n" ;

letter      = "a" | "b" | "c" | "d" | "e" | "f" | "g"
            | "h" | "i" | "j" | "k" | "l" | "m" | "n"
            | "o" | "p" | "q" | "r" | "s" | "t" | "u"
            | "v" | "w" | "x" | "y" | "z"
            | "A" | "B" | "C" | "D" | "E" | "F" | "G"
            | "H" | "I" | "J" | "K" | "L" | "M" | "N"
            | "O" | "P" | "Q" | "R" | "S" | "T" | "U"
            | "V" | "W" | "X" | "Y" | "Z" ;

digit       = "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9" ;
```

구현 메모:

- 키워드는 현재 구현상 소문자 리터럴만 허용한다. 예: `select`, `insert`
- `insert ... values ()` 같은 빈 value list는 허용하지 않는다.
- 문장 끝의 `;` 는 반드시 필요하다.
- 텍스트 값은 반드시 작은따옴표 문자열이어야 한다. 예: `'guest'`

---

# 기타

### 학습자료 찾은 방법

학습 자료 찾다가 이전에 스타 찍어둔 레포 https://github.com/utilForever/2025-MatKor-Make-SQLite 를 봤는데, 괜찮은 학습 자료를 찾음.

https://cstack.github.io/db_tutorial/ << 이게 되게 괜찮았음.

이거 기반으로 공부하고 필요한 부분만 따로 정리하기로 함.

굳이 뭐 실제 다른 DBMS가 어떻고 이럴 필요까지는 없고, 이거 자료 그대로 따라가면서 익히는것만 해도 충분할거 같음.


---

# README

빌드 산출물을 저장소 밖에 두려면 `sijun` 폴더에서 아래 명령을 사용한다.

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default
```

테스트 파일은 루트에 `test_main_unit.c`, `test_main_integration.c`로 둔다.

## `.sql` 파일로 실제 실행 테스트하기

프로그램은 표준 입력에서 한 줄씩 읽는다. 따라서 `.sql` 파일도 아래 규칙을 지켜야 한다.

- SQL은 한 줄에 하나씩 적는다.
- 모든 SQL 문장은 반드시 `;` 로 끝나야 한다.
- 현재 지원하는 쿼리는 `select * from ...;` 와 `insert into ... values (...);` 뿐이다.
- 종료는 SQL이 아니라 `.exit` 한 줄로 적는다.
- 일반 텍스트 줄은 SQL로 처리하지 않고 그대로 출력된다.

예시는 [`manual_test.sql`](/Users/sijun-yang/Documents/GitHub/jungle-week6/sijun/manual_test.sql) 를 보면 된다.

```sql
insert into users values (3, 'park', 'guest');
select * from users;
insert into posts values (12, 'draft', 'note');
select * from posts;
.exit
```

### 실행 방법

`repo root` 에서 아래처럼 실행하면 된다.

```bash
cd sijun
cmake --preset default
cmake --build --preset default
cd ..
/tmp/jungle-week6-sijun-build/sijun < sijun/manual_test.sql
```

또는 `sijun` 폴더 안에서 실행할 때는 기본 DB 경로가 상대경로 `sijun/db` 이므로, 작업 디렉터리를 `repo root` 로 맞춰서 실행해야 한다.

### 처리 방식

- 시작 시 `sijun/db/metadata.csv` 를 읽어서 테이블 메타데이터를 구성한다.
- `metadata.csv` 에 테이블이 하나도 없으면 시작 시 metadata error로 실패한다.
- `select` 또는 `insert` 로 시작하는 줄은 parser가 SQL로 해석한다.
- parse 성공 후 메타데이터 기준 semantic check를 수행한다.
- `insert` 는 `sijun/db/{table}.csv` 마지막에 한 줄을 append 한다.
- `select *` 는 `sijun/db/{table}.csv` 전체를 읽고 ASCII 표 형태로 stdout에 출력한다.
- `.exit` 를 만나면 프로그램을 종료한다.

### 확인 포인트

- `insert into users ...;` 실행 후 [`users.csv`](/Users/sijun-yang/Documents/GitHub/jungle-week6/sijun/db/users.csv) 마지막 줄이 늘어나는지 확인한다.
- `insert into posts ...;` 실행 후 [`posts.csv`](/Users/sijun-yang/Documents/GitHub/jungle-week6/sijun/db/posts.csv) 마지막 줄이 늘어나는지 확인한다.
- `select * from users;` 와 `select * from posts;` 는 모든 컬럼을 표 형태로 출력해야 한다.
- `;` 가 빠진 SQL은 `Parse error: expected ;` 로 실패해야 한다.

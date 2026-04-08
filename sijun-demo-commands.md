# Sijun Demo Commands

사전 준비: 아래 4개 명령은 `sijun` 바이너리가 이미 빌드되어 있다는 전제다. 아직 안 만들었다면 한 번만 `cmake -S sijun -B /tmp/jungle-week6-sijun-build && cmake --build /tmp/jungle-week6-sijun-build` 를 먼저 실행하면 된다.

1. 2개 테이블 조회

```bash
printf "select * from users;\nselect * from posts;\n.exit\n" | /tmp/jungle-week6-sijun-build/sijun
```

2. 데이터 1건 추가

```bash
printf "insert into posts values (12, 'draft', 'note');\n.exit\n" | /tmp/jungle-week6-sijun-build/sijun
```

3. 추가한 결과 조회

```bash
printf "select * from posts;\n.exit\n" | /tmp/jungle-week6-sijun-build/sijun
```

4. REPL을 다시 열어서 조회해서 영속성 확인

```bash
printf "select * from posts;\n.exit\n" | /tmp/jungle-week6-sijun-build/sijun
```

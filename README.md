# M1 ARM64 Mini OS

Apple Silicon Mac에서 만들고 QEMU의 ARM64 `virt` 머신에서 실행하는 최소 커널입니다.
부트로더 없이 ELF 커널을 직접 올리며, PL011 직렬 포트로 메시지를 출력합니다.

## LLM을 활용한 개발

이 프로젝트는 OpenAI Codex와 같은 LLM 코딩 에이전트를 개발 보조 도구로
활용합니다. LLM은 다음 작업을 돕습니다.

- 커널 구조와 다음 개발 단계 설계
- C 및 ARM64 어셈블리 코드 초안 작성
- 컴파일·링크·QEMU 실행 오류 분석
- 문서 작성과 코드 설명

LLM이 생성한 결과는 그대로 신뢰하지 않고 소스 검토, 컴파일, QEMU 부팅
테스트를 통해 확인합니다. LLM은 개발 과정에 사용될 뿐 현재 커널에 포함되어
실행되지는 않으며, 부팅한 OS가 네트워크로 모델에 접속하지도 않습니다.

## 준비

```sh
brew install llvm lld qemu
```

최신 Homebrew에서는 LLVM 링커인 `ld.lld`가 `llvm`과 별도의 `lld` 패키지에
들어 있습니다.

## 실행

```sh
make run
```

정상적으로 부팅되면 다음 문구가 보입니다.

```text
Hello from M1-made ARM64 OS!
Kernel is running on QEMU virt.
Type 'help' to see available commands.

mini-os>
```

QEMU를 종료하려면 `Control-A`, `X`를 차례로 누르세요.

## 셸 사용

`mini-os>` 프롬프트에 명령을 입력하고 Enter를 누릅니다. 입력 중에는
Backspace 또는 Delete로 문자를 지울 수 있습니다.

| 명령 | 기능 |
| --- | --- |
| `help` | 사용 가능한 명령 목록 출력 |
| `hello` | 인사말 출력 |
| `clear` | ANSI 이스케이프 코드로 터미널 지우기 |
| `info` | 아키텍처, 가상 머신, 콘솔 정보 출력 |
| `reboot` | PSCI를 통해 QEMU 가상 머신 재부팅 |

아직 구현되지 않은 명령을 입력하면 오류와 함께 `help` 사용법을 안내합니다.

## 구조

- `src/boot.S`: CPU 선택, BSS 초기화, 스택 설정
- `src/kernel.c`: 커널 진입점과 셸 시작
- `src/uart.c`: PL011 UART 문자 입력·출력
- `src/console.c`: 한 줄 입력, 문자 에코, Backspace 처리
- `src/shell.c`: 명령 프롬프트와 기본 명령
- `src/platform.c`: PSCI 기반 시스템 재부팅
- `linker.ld`: QEMU ARM64 메모리 배치
- `Makefile`: 크로스 컴파일과 실행

## 다음 단계

1. ARM64 예외 벡터와 인터럽트 처리
2. 시스템 타이머
3. 물리·동적 메모리 관리
4. 태스크 전환과 멀티태스킹
5. 파일 시스템
6. 사용자 프로그램

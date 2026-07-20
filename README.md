# ARM64 QEMU Mini OS

QEMU의 ARM64 `virt` 머신에서 실행하는 최소 커널입니다. 부트로더 없이 ELF
커널을 직접 올리며, PL011 직렬 포트로 명령을 입력하고 결과를 출력합니다.
GICv2 인터럽트 컨트롤러와 ARM Generic Timer를 사용해 100Hz 시스템 틱을
발생시키며, UART 수신은 IRQ와 256바이트 ring buffer로 처리합니다.

현재 개발 PC는 Apple Silicon M1 Mac이지만, 이 커널의 실행 대상은 M1
하드웨어가 아니라 QEMU ARM64 가상 머신입니다. 따라서 M1 전용 OS가 아니며,
필요한 LLVM·LLD·QEMU 도구를 사용할 수 있는 다른 컴퓨터에서도 빌드하고
실행할 수 있습니다.

## LLM을 활용한 개발

이 프로젝트는 OpenAI Codex와 같은 LLM 코딩 에이전트를 개발 보조 도구로
활용합니다. LLM은 다음 작업을 돕습니다.

- 커널 구조와 다음 개발 단계 설계
- C 및 ARM64 어셈블리 코드 초안 작성
- 컴파일·링크·QEMU 실행 오류 분석
- 문서 작성과 코드 설명

LLM이 생성한 결과는 소스 검토, 컴파일, QEMU 부팅
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
Hello from ARM64 QEMU Mini OS!
Kernel is running on QEMU virt.
Type 'help' to see available commands.

mini-os>
```

정상 종료하려면 셸에서 `shutdown`을 입력합니다. 언제든 QEMU를 강제로
종료하려면 `Control-A`, `X`를 차례로 누르세요.

## 셸 사용

`mini-os>` 프롬프트에 명령을 입력하고 Enter를 누릅니다. 입력 중에는
Backspace 또는 Delete로 문자를 지울 수 있습니다. 최근 8개의 비어 있지 않은
명령을 기억하며, 위·아래 방향키로 이전 명령을 탐색할 수 있습니다. 작성 중에
위 방향키를 눌렀다가 아래 방향키로 돌아오면 작성하던 문장이 복원됩니다. 한 줄은
최대 79자이며, 바로 앞 명령과 같은 명령을 연속 실행하면 한 번만 저장됩니다.

| 명령 | 기능 |
| --- | --- |
| `help` | 사용 가능한 명령 목록 출력 |
| `hello` | 인사말 출력 |
| `clear` | ANSI 이스케이프 코드로 터미널 지우기 |
| `info` | 아키텍처, 타이머, UART IRQ·버퍼 통계 등 시스템 정보 출력 |
| `ticks` | 부팅 후 발생한 100Hz 타이머 틱 수 출력 |
| `uptime` | 부팅 후 지난 시간을 초 단위로 출력 |
| `fault` | BRK 동기 예외를 발생시키고 예외 정보 출력 후 정지 |
| `shutdown` | PSCI를 통해 QEMU 가상 머신 전원 끄기 |
| `reboot` | PSCI를 통해 QEMU 가상 머신 재부팅 |

아직 구현되지 않은 명령을 입력하면 오류와 함께 `help` 사용법을 안내합니다.

`fault`는 ARM64 예외 처리기를 검증하기 위한 명령입니다. 실행하면
`ESR_EL1`, `ELR_EL1`, `FAR_EL1`, `SPSR_EL1` 값이 출력되고 시스템이
의도적으로 정지합니다. 다시 사용하려면 `Control-A`, `X`로 QEMU를 종료한 뒤
`make run`을 실행하세요.

## 예외 처리

커널은 시작할 때 2KB 정렬된 ARM64 예외 벡터 테이블을 `VBAR_EL1`에
등록합니다. 현재 EL의 동기 예외, IRQ, FIQ, SError와 낮은 EL에서 발생하는
예외를 위한 16개 진입점을 갖습니다.

IRQ가 발생하면 일반 레지스터와 `ELR_EL1`, `SPSR_EL1`을 스택에 저장하고 C
처리기를 호출합니다. 처리가 끝나면 모든 상태를 복원하고 `eret`으로 중단된
코드에 돌아갑니다. 동기 예외, FIQ, SError는 아직 복구하지 않으며 예외 정보를
UART로 보고한 뒤 추가 손상을 막기 위해 커널을 정지합니다.

## 인터럽트와 시스템 타이머

실행 시 QEMU 머신을 GICv2, 비보안 모드, CPU 1개로 고정합니다. 커널은 QEMU
`virt`의 GIC Distributor와 CPU Interface를 초기화하고 ARM Generic Timer의
비보안 물리 타이머(`CNTP_*`) PPI 30번을 연결합니다. 타이머는 초당 100번
IRQ를 발생시켜 틱 카운터를 증가시킵니다.

PL011 UART는 SPI 1, 즉 GIC INTID 33을 사용합니다. 커널은 UART를 115200 baud,
8-bit, FIFO 모드로 초기화하고 RX·receive-timeout·오류 인터럽트를 활성화합니다.
IRQ 처리기는 하드웨어 FIFO를 끝까지 비우며 문자를 실제 용량 256바이트인 ring
buffer에 넣습니다. 셸은 이 버퍼에서 문자를 소비하고, 비어 있으면 `wfe`로
기다립니다. ISR은 새 문자를 저장한 뒤 `sev`로 대기 중인 코드를 깨웁니다.

ring buffer가 가득 차면 새 문자를 버리되 하드웨어 FIFO는 계속 비워 IRQ가
반복되는 것을 막습니다. `info` 명령에서 UART IRQ 횟수, 수신 바이트 수, 현재
버퍼 사용량, 최대 사용량, drop 수와 하드웨어 오류 수를 확인할 수 있습니다.

## 구조

- `src/boot.S`: CPU 선택, BSS 초기화, 스택 설정
- `src/kernel.c`: 커널 진입점과 셸 시작
- `src/uart.c`: PL011 UART IRQ, 256바이트 RX ring buffer, 입출력
- `src/console.c`: 한 줄 입력, 편집, 방향키 명령 히스토리
- `src/shell.c`: 명령 프롬프트와 기본 명령
- `src/platform.c`: PSCI 기반 시스템 종료와 재부팅
- `src/exception_vectors.S`: 16개 ARM64 EL1 벡터, IRQ 프레임과 `eret`
- `src/exception.c`: 예외 원인과 시스템 레지스터 UART 보고
- `src/gic.c`: QEMU `virt`의 GICv2 초기화와 IRQ acknowledge/EOI
- `src/irq.c`: 타이머·UART IRQ 분배와 IRQ 마스크 해제
- `src/timer.c`: ARM Generic Timer 초기화와 100Hz 틱 카운터
- `linker.ld`: QEMU ARM64 메모리 배치
- `Makefile`: 크로스 컴파일과 실행

## 현재 한계

- QEMU `virt`의 GICv2 주소를 직접 사용하며 Device Tree를 해석하지 않습니다.
- CPU 1개만 실행하며 UART 주소·24MHz clock·GIC INTID를 직접 지정합니다.
- 256바이트보다 큰 순간 입력은 일부 문자가 drop될 수 있으며 통계에 기록됩니다.
- 동기 예외, FIQ, SError는 복구하지 않고 진단 후 정지합니다.
- MMU, 동적 메모리, 프로세스, 파일 시스템, 사용자 프로그램은 없습니다.

## 다음 단계

1. 명령 인자와 공백 처리 (`echo hello`, `sleep 3`)
2. 자동 QEMU 부팅·셸 회귀 테스트
3. 예외·IRQ 진단 강화
4. 물리·동적 메모리 관리와 MMU
5. 태스크 전환과 멀티태스킹
6. 파일 시스템과 사용자 프로그램

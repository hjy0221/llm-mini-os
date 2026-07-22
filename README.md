# ARM64 QEMU Mini OS

[한국어](#arm64-qemu-mini-os) · [English](#english)

QEMU의 ARM64 `virt` 머신에서 실행하는 최소 커널입니다. 부트로더 없이 ELF
커널을 직접 올리며, PL011 직렬 포트로 명령을 입력하고 결과를 출력합니다.
GICv2 인터럽트 컨트롤러와 ARM Generic Timer를 사용해 100Hz 시스템 틱을
발생시키며, UART 수신은 IRQ와 256바이트 ring buffer로 처리합니다. ARM64
MMU의 identity mapping, 4KiB 물리 페이지 할당기, 명시적인 `yield`로 전환하는
협력형 커널 태스크도 포함합니다.

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

macOS에서는 Homebrew로 설치합니다.

```sh
brew install llvm lld qemu python
```

최신 Homebrew에서는 LLVM 링커인 `ld.lld`가 `llvm`과 별도의 `lld` 패키지에
들어 있습니다.

Ubuntu에서는 다음 패키지를 설치합니다.

```sh
sudo apt-get update
sudo apt-get install clang lld qemu-system-arm python3
```

Makefile은 macOS에서는 Homebrew 경로를 사용하고, 그 밖의 환경에서는 `PATH`의
`clang`, `ld.lld`, `qemu-system-aarch64`, `python3`를 찾습니다. 선택된 도구는
`make print-tools`로 확인할 수 있습니다.

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

## 자동 테스트

다음 명령은 커널을 새로 빌드하고 QEMU를 자동으로 부팅한 뒤 UART 셸을
검사합니다. 별도의 Python 패키지는 필요하지 않습니다.

```sh
make test
```

테스트는 다음 기능을 한 QEMU 세션에서 확인합니다.

- 커널 부팅 문구와 셸 프롬프트
- 기본 명령, 공백·따옴표·backslash 인자 분리와 최대 인자 처리
- 닫히지 않은 따옴표와 줄 끝 backslash parse error
- 위험한 추가 인자가 붙은 `fault`, `shutdown`, `reboot` 차단
- `sleep` 숫자 검사와 100Hz 타이머 틱
- MMU 활성화와 translation table 정렬·속성
- 물리 페이지 할당·zero-fill·반환·double-free 방어
- 두 커널 태스크의 결정적인 round-robin context switch와 stack 회수
- Backspace와 위·아래 방향키 명령 기록 및 작성 중 문장 복원
- `sleep` 도중 UART ring buffer에 들어온 명령 보존
- UART IRQ·drop·오류 통계
- PSCI 정상 종료와 QEMU 종료 코드

성공하면 각 항목이 `[PASS]`로 표시됩니다. 전체 UART 기록은
`build/qemu-test.log`에 저장됩니다. GitHub에 push하거나 pull request를 만들면
GitHub Actions의 Ubuntu 환경에서도 같은 `make test`가 자동 실행됩니다.

## 셸 사용

`mini-os>` 프롬프트에 명령을 입력하고 Enter를 누릅니다. 입력 중에는
Backspace 또는 Delete로 문자를 지울 수 있습니다. 최근 8개의 비어 있지 않은
명령을 기억하며, 위·아래 방향키로 이전 명령을 탐색할 수 있습니다. 작성 중에
위 방향키를 눌렀다가 아래 방향키로 돌아오면 작성하던 문장이 복원됩니다. 한 줄은
최대 79자이며, 바로 앞 명령과 같은 명령을 연속 실행하면 한 번만 저장됩니다.
명령 이름과 인자는 따옴표 밖의 공백으로 구분합니다. 앞뒤 공백과 연속된 공백을
허용하며, 명령 이름을 포함해 최대 8개 인자를 입력할 수 있습니다. 큰따옴표와
작은따옴표 안의 공백은 한 인자에 포함됩니다. `\`는 바로 다음 문자를 문자 그대로
포함하므로 공백, 큰따옴표, 작은따옴표와 `\`를 인자에 넣을 수 있습니다.

```text
mini-os> echo "hello world" 'from mini os'
hello world from mini os
mini-os> echo escaped\ space \"quoted\" \\backslash
escaped space "quoted" \backslash
mini-os> sleep 1
Sleeping for 1 second...
Done.
mini-os> taskdemo 3
Cooperative task demo:
Task A: 1
Task B: 1
Task A: 2
Task B: 2
Task A: 3
Task B: 3
taskdemo: PASS
```

| 명령 | 기능 |
| --- | --- |
| `help` | 사용 가능한 명령 목록 출력 |
| `hello` | 인사말 출력 |
| `echo <text...>` | 입력한 인자를 공백 하나로 연결해 출력 |
| `sleep <seconds>` | 지정한 초 동안 기다린 뒤 셸로 복귀 (0~86400) |
| `mem` | 4KiB 물리 페이지의 전체·사용·여유 개수 출력 |
| `memtest` | 페이지 3개를 할당·검사·반환하고 double-free 방어 확인 |
| `mmu` | MMU, cache, TTBR0, MAIR, TCR 상태 출력 |
| `tasks` | 협력형 scheduler와 context switch 누적 통계 출력 |
| `taskdemo [rounds]` | A/B 커널 태스크를 만들어 round-robin 전환 (기본 3, 최대 20) |
| `clear` | ANSI 이스케이프 코드로 터미널 지우기 |
| `info` | 아키텍처, 타이머, UART IRQ·버퍼 통계 등 시스템 정보 출력 |
| `ticks` | 부팅 후 발생한 100Hz 타이머 틱 수 출력 |
| `uptime` | 부팅 후 지난 시간을 초 단위로 출력 |
| `fault` | BRK 동기 예외를 발생시키고 예외 정보 출력 후 정지 |
| `shutdown` | PSCI를 통해 QEMU 가상 머신 전원 끄기 |
| `reboot` | PSCI를 통해 QEMU 가상 머신 재부팅 |

아직 구현되지 않은 명령을 입력하면 오류와 함께 `help` 사용법을 안내합니다.
파서는 큰따옴표와 작은따옴표 자체는 결과에서 제거하고, 따옴표로 감싼 부분과
바로 붙은 일반 문자를 같은 인자로 연결합니다. 예를 들어
`echo pre"hello world"post`는 `prehello worldpost`를 출력합니다. 따옴표 안에서도
backslash로 다음 문자를 이스케이프할 수 있습니다. 닫히지 않은 따옴표나 줄 끝의
backslash는 명확한 parse error를 출력하며 해당 명령을 실행하지 않습니다.

`sleep`은 타이머 값을 반복해서 읽으며 CPU를 계속 사용하는 busy-wait 방식이
아닙니다. `wfe`로 CPU를 쉬게 하고 100Hz 타이머 IRQ가 깨울 때마다 경과 시간을
확인합니다. 기다리는 동안 도착한 UART 입력은 ring buffer에 저장됩니다.

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

## 메모리 관리와 MMU

QEMU `virt`의 128MiB RAM은 `0x40000000~0x47ffffff`에 있습니다. 커널 image,
정적 데이터, 64KiB boot stack 뒤의 첫 4KiB 경계를 linker symbol
`__kernel_end`로 표시하고, 그 다음 페이지부터 물리 페이지 allocator가
관리합니다. 새 페이지는 0으로 지운 뒤 반환하며, 반환된 페이지는 free-list에서
먼저 재사용합니다. `NULL`, 비정렬 주소, 범위 밖 주소와 double-free는
거부합니다.

MMU는 4KiB translation granule과 2MiB block descriptor를 사용합니다. 현재
가상 주소와 물리 주소를 같게 두는 identity mapping이므로 MMU를 켠 뒤에도
커널, stack, UART, GIC 주소가 바뀌지 않습니다. GIC와 UART는
Device-nGnRnE로, RAM은 Normal WBWA 속성으로 분리합니다. TTBR0용 L1 table 한
장, 장치용 L2 한 장, RAM용 L2 한 장으로 총 12KiB의 정적 translation table을
사용합니다.

현재 단계에서는 `SCTLR_EL1.M`만 켜서 주소 변환을 활성화합니다. D-cache와
I-cache는 안전한 cache invalidate와 페이지별 실행 권한을 구현하기 전까지
의도적으로 끈 상태입니다. `mmu` 명령으로 실제 register 값을 확인할 수
있습니다.

## 협력형 커널 태스크

태스크 slot은 boot·shell 태스크를 포함해 최대 4개입니다. 새 태스크는 물리
페이지 allocator에서 4KiB stack 한 장을 받고, `task_yield()`를 호출할 때만
다음 READY 태스크로 전환됩니다. ARM64 호출 규약에 따라 x19~x30과 SP를
assembly에서 저장·복원하며, C 구조체 offset은 `_Static_assert`로 검사합니다.
빌드 옵션 `-mgeneral-regs-only`를 유지하므로 현재 FP/SIMD context는 저장하지
않습니다. 각 stack의 가장 아래에는 canary를 두고 `yield`, 종료, 회수 시
손상 여부를 확인합니다.

태스크 entry 함수가 끝나면 FINISHED 상태가 되고, 자신의 stack에서는 그
페이지를 해제하지 않습니다. shell 태스크로 돌아온 뒤에만 stack을 회수해 현재
실행 중인 stack을 잘못 반환하는 일을 피합니다. `taskdemo`는 두 태스크의 지역
상태가 context switch 뒤에도 유지되는지 보여주며, `tasks`에서 생성·전환·종료·
회수 횟수를 볼 수 있습니다. 이 단계의 태스크는 모두 EL1과 같은 주소 공간에서
실행하는 커널 태스크이며 프로세스 격리는 아닙니다.

## 구조

- `src/boot.S`: CPU 선택, BSS 초기화, 스택 설정
- `src/kernel.c`: 커널 진입점과 셸 시작
- `src/uart.c`: PL011 UART IRQ, 256바이트 RX ring buffer, 입출력
- `src/console.c`: 한 줄 입력, 편집, 방향키 명령 히스토리
- `src/shell.c`: 명령·인자 파서, 명령 프롬프트와 기본 명령
- `src/platform.c`: PSCI 기반 시스템 종료와 재부팅
- `src/exception_vectors.S`: 16개 ARM64 EL1 벡터, IRQ 프레임과 `eret`
- `src/exception.c`: 예외 원인과 시스템 레지스터 UART 보고
- `src/gic.c`: QEMU `virt`의 GICv2 초기화와 IRQ acknowledge/EOI
- `src/irq.c`: 타이머·UART IRQ 분배와 IRQ 마스크 해제
- `src/timer.c`: ARM Generic Timer 초기화와 100Hz 틱 카운터
- `src/mmu.c`: 4KiB translation table, RAM/MMIO identity mapping과 MMU 활성화
- `src/page_alloc.c`: 4KiB 물리 페이지 bump/free-list allocator
- `src/task.c`: 협력형 round-robin scheduler, 태스크 생성·종료·stack 회수
- `src/task_switch.S`: ARM64 x19~x30·SP context switch
- `tests/qemu_smoke.py`: QEMU 부팅과 UART 셸 자동 회귀 테스트
- `.github/workflows/test.yml`: push·pull request용 Linux 자동 테스트
- `linker.ld`: QEMU ARM64 메모리 배치
- `Makefile`: macOS·Linux 도구 탐색, 크로스 컴파일, 실행과 테스트

## 현재 한계

- QEMU `virt`의 GICv2 주소를 직접 사용하며 Device Tree를 해석하지 않습니다.
- CPU 1개만 실행하며 UART 주소·24MHz clock·GIC INTID를 직접 지정합니다.
- 256바이트보다 큰 순간 입력은 일부 문자가 drop될 수 있으며 통계에 기록됩니다.
- 명령 파서는 환경 변수·명령 치환·와일드카드 확장을 지원하지 않습니다.
- 동기 예외, FIQ, SError는 복구하지 않고 진단 후 정지합니다.
- MMU는 identity mapping과 2MiB block만 사용하며 페이지별 R/O·NX·guard 권한이
  없습니다. D-cache와 I-cache도 아직 활성화하지 않습니다.
- 페이지 allocator는 고정된 128MiB QEMU RAM과 4KiB 단위만 관리하며 작은 객체용
  heap allocator는 없습니다.
- scheduler는 명시적으로 `yield`하는 협력형입니다. 타이머 선점, 우선순위,
  scheduler-aware UART 대기·sleep은 아직 없습니다.
- 태스크 stack은 현재 4KiB 한 장이며 canary 진단은 있지만 MMU guard page나
  별도의 IRQ stack은 아직 없습니다.
- 모든 태스크가 EL1의 같은 주소 공간을 공유하며 프로세스 격리, 파일 시스템,
  사용자 프로그램은 없습니다.

## 다음 단계

1. L3 page table로 `.text`·`.rodata`·stack 권한 분리와 guard page 추가
2. cache 유지보수 뒤 D-cache·I-cache 활성화
3. UART·sleep을 scheduler-aware 대기로 바꾸고 타이머 선점 추가
4. 작은 객체용 kernel heap과 사용자 주소 공간
5. 파일 시스템과 사용자 프로그램

---

## English

ARM64 QEMU Mini OS is a small freestanding kernel for QEMU's ARM64 `virt` machine. It boots an ELF kernel directly, exposes an interactive shell over PL011 UART, and uses GICv2 plus the ARM Generic Timer for a 100 Hz system tick.

### Implemented features

- ARM64 boot code, linker layout, BSS initialization, and a 64 KiB boot stack
- Interrupt-driven PL011 UART input with a 256-byte ring buffer
- EL1 exception vectors and diagnostic reporting for synchronous exceptions
- GICv2 interrupt handling and a 100 Hz generic timer
- 4 KiB physical page allocator with reuse and double-free protection
- MMU identity mappings for RAM and MMIO with separate memory attributes
- Cooperative round-robin kernel tasks with context switching and stack canaries
- Interactive shell with quoting, escaping, history, editing, and system commands
- Automated QEMU smoke tests and GitHub Actions CI

### Requirements

- Clang and LLD with the `aarch64-none-elf` target
- `qemu-system-aarch64`
- Python 3 for the smoke test

On macOS, the required tools can be installed with Homebrew:

```bash
brew install llvm lld qemu python
```

### Build, run, and test

```bash
make
make run
make test
```

Exit QEMU with `Control-A`, then `X`.

### Shell commands

The shell includes `help`, `hello`, `echo`, `sleep`, `mem`, `memtest`, `mmu`, `tasks`, `taskdemo`, `clear`, `info`, `ticks`, `uptime`, `fault`, `shutdown`, and `reboot`.

### LLM-assisted development

LLM coding agents are used for design exploration, C and ARM64 assembly drafts, build and QEMU troubleshooting, tests, and documentation. Generated changes are reviewed through source inspection, compiler warnings, runtime checks, and automated smoke tests before acceptance.

### Current limitations

- The platform is fixed to a single-core QEMU `virt` machine with hard-coded GIC and UART settings.
- MMU mappings use 2 MiB identity-mapped blocks; caches and per-page permissions are not enabled yet.
- Scheduling is cooperative, and every kernel task shares the same EL1 address space.
- There is no small-object heap, filesystem, process isolation, or user program support yet.

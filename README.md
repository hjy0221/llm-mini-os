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
```

QEMU를 종료하려면 `Control-A`, `X`를 차례로 누르세요.

## 구조

- `src/boot.S`: CPU 선택, BSS 초기화, 스택 설정
- `src/kernel.c`: 커널 진입점과 UART 출력
- `linker.ld`: QEMU ARM64 메모리 배치
- `Makefile`: 크로스 컴파일과 실행

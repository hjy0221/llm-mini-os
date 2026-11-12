# ARM64 QEMU Mini OS

[日本語](https://github.com/hjy0221/llm-mini-os#%E6%97%A5%E6%9C%AC%E8%AA%9E) · [English](https://github.com/hjy0221/llm-mini-os#english) · [한국어](https://github.com/hjy0221/llm-mini-os#%ED%95%9C%EA%B5%AD%EC%96%B4)

## 日本語

QEMUのARM64 `virt`マシンで実行される最小カーネル。ブートローダなしでELF
カーネルを直接上げ、PL011シリアルポートにコマンドを入力して結果を出力します。
GICv2割り込みコントローラとARM Generic Timerを使用した100Hzシステムティック
発生し、UART受信はIRQと256バイトのリングバッファで処理されます。 ARM64
MMUのidentity mapping、4KiB物理ページアロケータ、明示的な`yield`に切り替える
コラボレーションカーネルタスクも含まれています。

現在開発PCはApple Silicon M1 Macですが、このカーネルの実行対象はM1
ハードウェアではなく、QEMU ARM64仮想マシンです。したがって、M1専用OSではなく、
必要なLLVM・LLD・QEMUツールを使用できる他のコンピューターでもビルドし、
実行できます。

## LLMを活用した開発

このプロジェクトは、OpenAI CodexなどのLLMコーディングエージェントを開発支援ツールとして
活用します。 LLM は次の作業を支援します。

- カーネル構造と次の開発段階の設計
- CおよびARM64アセンブリコードドラフトの作成
- コンパイル・リンク・QEMU実行エラー解析
- 文書の作成とコードの説明

LLMが生成した結果は、ソースレビュー、コンパイル、QEMUブート
テストを通して確認してください。 LLMは開発プロセスに使用され、現在カーネルに含まれています
実行されず、起動したOSがネットワークでモデルに接続することもありません。

## 準備

macOSではHomebrewとしてインストールします。

```sh
brew install llvm lld qemu python
```

最新のHomebrewでは、LLVMリンカーである`ld.lld`が`llvm`とは別の`lld`パッケージに
含まれています。

Ubuntuは次のパッケージをインストールします。

```sh
sudo apt-get update
sudo apt-get install clang lld qemu-system-arm python3
```

MakefileはmacOSではHomebrewパスを使用し、その他の環境では`PATH`の
`clang`、`ld.lld`、`qemu-system-aarch64`、`python3`を探します。選択したツールは
`make print-tools`で確認できます。

## 実行

```sh
make run
```

正常に起動すると、次のフレーズが表示されます。

```text
Hello from ARM64 QEMU Mini OS!
Kernel is running on QEMU virt.
Type 'help' to see available commands.

mini-os>
```

正常終了するには、シェルで`shutdown`と入力します。いつでもQEMUを強制的に
終了するには、`Control-A`、`X`を順に押します。

## 自動テスト

次のコマンドはカーネルを新しくビルドし、QEMUを自動的に起動し、UARTシェルを
検査します。別のPythonパッケージは必要ありません。

```sh
make test
```

テストは、次の機能を1つのQEMUセッションで確認します。

- カーネルブートフレーズとシェルプロンプト
- 基本コマンド、空白・引用符・backslash引数分離と最大引数処理
- 閉じていない引用符と行末backslash parse error
- 危険な追加因子が付いた`fault`、`shutdown`、`reboot`ブロック
- `sleep`数字チェックと100Hzタイマーティック
- MMU活性化と翻訳テーブルのソート・プロパティ
- 物理ページ割り当て・zero-fill・返却・double-free防御
- 2つのカーネルタスクの決定的なラウンドロビンコンテキストスイッチとスタックの回収
- Backspaceと上・下方向キーコマンド記録および作成中の文章復元
- `sleep`中にUARTリングバッファに入ったコマンドの保存
- UART IRQ・drop・エラー統計
- PSCI正常終了とQEMU終了コード

成功すると、各項目は`[PASS]`としてマークされます。フルUARTレコードは
`build/qemu-test.log`に保存されます。 GitHubにプッシュするかプルリクエストを作成すると
GitHub ActionsのUbuntu環境でも同じ`make test`が自動的に実行されます。

## シェルの使用

`mini-os>` プロンプトでコマンドを入力し、Enter キーを押します。入力中は
Backspace または Delete で文字を消去できます。最近8つの空ではありません
コマンドを記憶し、上・下方向キーで前のコマンドをナビゲートできます。作成中
上方向キーを押して下方向キーに戻ると、作成した文が復元されます。一行
最大79文字で、直前のコマンドなどのコマンドを連続して実行すると一度だけ保存されます。
コマンド名と引数は、引用符の外のスペースで区切られます。前後のスペースと連続したスペース
許可し、コマンド名を含む最大8つの引数を入力できます。二重引用符と
一重引用符内のスペースは、1つの引数に含まれています。 `\`はすぐに次の文字を文字通り
含めるので、スペース、二重引用符、一重引用符、`\`を引数に入れることができます。

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

|コマンド機能
| --- | --- |
| `help` |利用可能なコマンドリスト出力|
| `hello` |グリーティング出力|
| `echo <text...>` |入力した引数を空白1つに連結して出力
| `sleep <seconds>` |指定された秒待ってからシェルに戻る（0〜86400）|
| `mem` | 4KiB物理ページの全体・使用・余裕個数出力
| `memtest` |ページ3個を割り当て・検査・返却してdouble-free防御確認
| `mmu` | MMU、cache、TTBR0、MAIR、TCRステータス出力|
| `tasks` |協調型スケジューラとコンテキストスイッチ累積統計出力
| `taskdemo [rounds]` | A / Bカーネルタスクを作成してラウンドロビン切り替え（デフォルト3、最大20）|
| `clear` | ANSIエスケープコードで端末を消去する
| `info` |アーキテクチャ、タイマー、UART IRQ・バッファ統計などシステム情報出力
| `ticks` |起動後に発生した100Hzタイマチック数出力|
| `uptime` |起動後過去の時間を秒単位で出力|
| `fault` | BRK同期例外を発生させ、例外情報出力後に停止
| `shutdown` | PSCI経由でQEMU仮想マシンの電源を切る
| `reboot` | PSCI経由でQEMU仮想マシンを再起動する

まだ実装されていないコマンドを入力すると、エラーとともに`help`の使用方法を案内します。
パーサは二重引用符と一重引用符自体を結果から削除し、引用符で囲んだ部分と
すぐに付けられた一般文字を同じ引数に連結します。例えば
`echo pre"hello world"post`は`prehello worldpost`を出力します。引用符の中でも
backslash で次の文字をエスケープできます。閉じていない引用符または行末
backslash は明確な parse error を出力し、そのコマンドを実行しません。

`sleep`はタイマー値を繰り返し読み込み、CPUを使い続けるbusy-wait方式が
いいえ。 `wfe`でCPUを休ませ、100HzタイマーIRQが起きるたびに経過時間を
確認してください。待機中に到着したUART入力はリングバッファに保存されます。

`fault`は、ARM64例外ハンドラを検証するためのコマンドです。実行すると
`ESR_EL1`、`ELR_EL1`、`FAR_EL1`、`SPSR_EL1`の値が出力され、システム
意図的に停止します。再利用するには、`Control-A`、`X`でQEMUを終了してから
`make run`を実行してください。

## 例外処理

カーネルは、起動時に2KBソートされたARM64例外ベクトルテーブルを`VBAR_EL1`に登録します。現在のELの同期例外、IRQ、FIQ、SError、および低ELで発生
例外のための16のエントリポイントがあります。

IRQが発生した場合、汎用レジスタと`ELR_EL1`、`SPSR_EL1`をスタックに保存し、C
ハンドラを呼び出します。処理が完了すると、すべての状態が復元され、`eret`で中断されました
コードに戻ります。同期例外、FIQ、SErrorはまだ回復しておらず、例外情報
UARTで報告した後、追加の損傷を防ぐためにカーネルを停止します。

## 割り込みとシステムタイマ

実行時にQEMUマシンをGICv2、非セキュアモード、CPU1台に固定します。カーネルはQEMU
`virt`のGIC DistributorとCPU Interfaceを初期化し、ARM Generic Timerの
非セキュア物理タイマー（`CNTP_*`）PPI 30番を接続します。タイマーは毎秒100回
IRQを発生させ、ティックカウンタをインクリメントします。

PL011 UARTはSPI 1、つまりGIC INTID 33を使用します。カーネルはUARTを115200 baud、
8-bit、FIFOモードで初期化し、RX・receive-timeout・エラー割り込みを有効にします。
IRQハンドラはハードウェアFIFOを最後まで空にし、文字を実容量256バイトのリング
bufferに入れます。シェルはこのバッファで文字を消費し、空の場合は`wfe`
待ちます。 ISRは新しい文字を保存し、`sev`で待機しているコードを起動します。

リングバッファがいっぱいになると、新しい文字が破棄されますが、ハードウェアFIFOは空白になり、IRQ
繰り返されるのを防ぎます。 `info` コマンドでの UART IRQ 回数、受信バイト数、現在
バッファ使用量、最大使用量、ドロップ数、ハードウェアエラー数を確認できます。

## メモリ管理とMMU

QEMU `virt`の128MiB RAMは`0x40000000~0x47ffffff`にあります。カーネルイメージ、
静的データ、64KiBブートスタックの後ろの最初の4KiB境界をlinker symbol
`__kernel_end`としてマークし、次のページから物理ページのアロケータ
管理します。新しいページはゼロで消去され、返されたページはフリーリストから
まず再利用します。 `NULL`、非整列アドレス、範囲外アドレスとdouble-free
拒否します。

MMUは4KiB変換グランルと2MiBブロック記述子を使用します。現在
仮想アドレスと物理アドレスを同じにするidentity mappingなので、MMUをオンにしても
カーネル、スタック、UART、GICアドレスは変わりません。 GICとUARTは
Device-nGnRnEでは、RAMはNormal WBWA属性に分割されます。 TTBR0用L1テーブルハン
章、デバイス用L2一枚、RAM用L2一枚で合計12KiBの静的変換テーブルを
使用します。

現在の手順では、`SCTLR_EL1.M`のみをオンにしてアドレス変換を有効にします。 D-cacheと
I-cacheは、安全なcache invalidateとページ固有の実行権限を実装するまで
意図的にひも状態です。 `mmu`コマンドで実際のレジスタ値を確認できます
あります。

## 協調型カーネルタスク

タスク slot は boot・shell タスクを含めて最大 4 個です。新しいタスクは物理です
ページアロケータから4KiB stackを1枚受け取り、`task_yield()`を呼び出すときのみ
次のREADYタスクに切り替えます。 ARM64呼び出し規約に基づいてx19〜x30とSP
assemblyで保存・復元し、C構造体offsetは`_Static_assert`で検査します。
ビルドオプション`-mgeneral-regs-only`を維持するため、現在のFP / SIMDコンテキストは保存されません
ではありません。各スタックの一番下にはカナリーを置き、`yield`、終了、回収時損傷がないか確認してください。

タスク entry 関数が終わると FINISHED 状態になり、自身の stack ではその
ページを解放しません。 shellタスクに戻った後にのみstackを回収して現在
実行中のスタックを誤って返すことを避けます。 `taskdemo`は2つのタスクの領域です
状態が context switch の後にも維持されるかを示し、`tasks` で生成・切替・終了・
回数回数を見ることができます。このステップのタスクは、すべてEL1などのアドレス空間で
実行するカーネルタスクであり、プロセス分離ではありません。

## 構造

- `src/boot.S`：CPU選択、BSS初期化、スタック設定
- `src/kernel.c`: カーネルエントリポイントとシェル開始
- `src/uart.c`：PL011 UART IRQ、256バイトRXリングバッファ、入出力
- `src/console.c`：1行入力、編集、方向キーコマンド履歴
- `src/shell.c`: コマンド・引数パーサ、コマンドプロンプトと基本コマンド
- `src/platform.c`：PSCIベースのシステムシャットダウンと再起動
- `src/exception_vectors.S`：16個のARM64 EL1ベクトル、IRQフレームと`eret`
- `src/exception.c`：例外原因とシステムレジスタUART報告
- `src/gic.c`: QEMU `virt`のGICv2初期化とIRQ acknowledge/EOI
- `src/irq.c`: タイマー・UART IRQ分配とIRQマスク解除
- `src/timer.c`: ARM Generic Timer初期化と100Hzティックカウンター
- `src/mmu.c`: 4KiB translation table、RAM/MMIO identity mappingとMMU有効
- `src/page_alloc.c`: 4KiB 物理ページ bump/free-list allocator
- `src/task.c`: 協力型 round-robin scheduler、タスク生成・終了・stack回収
- `src/task_switch.S`: ARM64 x19～x30・SP context switch
- `tests/qemu_smoke.py`：QEMUブートとUARTシェルの自動回帰テスト
- `.github/workflows/test.yml`: push・pull request用のLinux自動テスト
- `linker.ld`: QEMU ARM64 メモリー配置
- `Makefile`: macOS・Linuxツールナビゲーション、クロスコンパイル、実行とテスト

## 現在の制限

- QEMU `virt`のGICv2アドレスを直接使用し、デバイスツリーを解釈しません。
- CPU 1個のみ実行し、UARTアドレス・24MHzクロック・GIC INTIDを直接指定します。
- 256バイトを超える瞬間入力は、一部の文字がドロップされ、統計に記録されます。
- コマンドパーサは環境変数・命令置換・ワイルドカード拡張をサポートしません。
- 同期例外、FIQ、SErrorは回復せず、診断後に停止します。
- MMUはidentity mappingと2MiB blockのみを使用し、ページ別R/O・NX・guard権限が
  ありません。 D-cacheとI-cacheもまだアクティブではありません。
- ページアロケータは固定された128MiB QEMU RAMと4KiB単位のみを管理し、小さなオブジェクト用
  heap allocator はありません。
- schedulerは明示的に`yield`する協力型です。タイマープリエンプション、優先順位、
  scheduler-aware UART 待機・スリープはまだありません。
- タスクスタックは現在4KiB一枚でカナリー診断はあるがMMU guard pageや
  別のIRQスタックはまだありません。
- すべてのタスクがEL1の同じアドレス空間を共有し、プロセス分離、ファイルシステム、
  ユーザープログラムはありません。

## 次のステップ

1. L3 page tableで`.text`・`.rodata`・stack権限分離とguard pageを追加
2. cacheメンテナンス後、D-cache・I-cache活性化
3. UART・sleepをscheduler-aware待機に切り替えてタイマープリエンプションを追加
4. 小さなオブジェクト用のカーネルヒープとユーザーアドレス空間
5.ファイルシステムとユーザープログラム

## English

A minimal kernel running on an ARM64 `virt` machine on QEMU. ELF without bootloader
It loads the kernel directly, inputs commands through the PL011 serial port, and outputs the results.
100Hz system ticks using GICv2 interrupt controller and ARM Generic Timer
UART reception is processed using IRQ and 256-byte ring buffer. ARM64
MMU's identity mapping, 4KiB physical page allocator, switching to explicit `yield`
Also includes cooperative kernel tasks.

The current development PC is an Apple Silicon M1 Mac, but this kernel runs on the M1
It's not hardware, it's a QEMU ARM64 virtual machine. Therefore, it is not a dedicated OS for M1,
Build on other computers that have the necessary LLVM, LLD, and QEMU tools available.
You can run it.

## Development using LLM

This project uses LLM coding agents such as OpenAI Codex as a development aid.
Take advantage of it. An LLM helps you:

- Design of kernel structure and next development steps
- Drafting C and ARM64 assembly code
- Analysis of compilation·link·QEMU execution errors
- Documentation and code explanation

The results generated by the LLM are source review, compilation, and QEMU booting.
Verify through testing. LLM is only used in the development process and is not included in the current kernel.
It does not run, and the booted OS does not connect to the model over the network.

## ready

On macOS, install with Homebrew.

```sh
brew install llvm lld qemu python
```

In the latest Homebrew, the LLVM linker `ld.lld` is in a separate package from `llvm`, `lld`.
It's in.

On Ubuntu, install the following packages:

```sh
sudo apt-get update
sudo apt-get install clang lld qemu-system-arm python3
```

The Makefile uses the Homebrew path on macOS, and `PATH` in other environments.
Look for `clang`, `ld.lld`, `qemu-system-aarch64`, and `python3`. The selected tool is
You can check it with `make print-tools`.

## run

```sh
make run
```

If it boots normally, you will see the following message:

```text
Hello from ARM64 QEMU Mini OS!
Kernel is running on QEMU virt.
Type 'help' to see available commands.

mini-os>
```

To perform a graceful shutdown, type `shutdown` in the shell. Force QEMU at any time
To exit, press `Control-A`, then `X`.

## Automatic testing

The following command builds a new kernel, automatically boots QEMU, and launches the UART shell.
Check. No separate Python package is required.

```sh
make test
```

The test checks the following functions in one QEMU session:

- Kernel boot phrase and shell prompt
- Basic commands, space/quote/backslash argument separation and maximum argument handling
- Unclosed quotes and line endings backslash parse error
- Block `fault`, `shutdown`, `reboot` with dangerous additional parameters
- `sleep` digit check and 100Hz timer ticks
- MMU activation and translation table alignment/properties
- Physical page allocation, zero-fill, return, double-free defense
- Deterministic round-robin context switch and stack recovery of two kernel tasks
- Record Backspace and up/down arrow key commands and restore sentences while writing
- Preservation of commands entered in UART ring buffer during `sleep`
- UART IRQ·drop·error statistics
- PSCI normal exit and QEMU exit code

If successful, each entry will be marked as `[PASS]`. The full UART history is
It is saved in `build/qemu-test.log`. When you push to GitHub or make a pull request
The same `make test` runs automatically in the Ubuntu environment in GitHub Actions.

## Use the shell

At the `mini-os>` prompt, type the command and press Enter. While typing
You can erase characters with Backspace or Delete. Last 8 non-empty
It remembers commands and allows you to navigate to previous commands using the up and down arrow keys. While writing
If you press the up arrow key and then return to the down arrow key, the sentence you were writing will be restored. One line
It can be up to 79 characters, and is saved only once if the same command as the previous command is executed consecutively.
Command names and arguments are separated by spaces outside the quotes. Leading and trailing spaces and consecutive spaces
Up to 8 arguments can be entered, including the command name. With double quotation marks
Spaces within single quotes are included in one argument. `\` literally matches the immediately following characters
Contains spaces, double quotes, single quotes, and `\` as arguments.

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

| command | Features |
| --- | --- |
| `help` | Print list of available commands |
| `hello` | print greeting |
| `echo <text...>` | Prints the input arguments concatenated with one space |
| `sleep <seconds>` | Wait for the specified number of seconds and return to the shell (0~86400) |
| `mem` | Output total/used/free number of 4KiB physical pages |
| `memtest` | Allocate, inspect, and return 3 pages and check double-free defense |
| `mmu` | MMU, cache, TTBR0, MAIR, TCR status output |
| `tasks` | Collaborative scheduler and context switch cumulative statistics output |
| `taskdemo [rounds]` | Create A/B kernel tasks to switch round-robin (default 3, maximum 20) |
| `clear` | Clearing terminal with ANSI escape code |
| `info` | Output system information such as architecture, timer, UART IRQ and buffer statistics |
| `ticks` | Output the number of 100Hz timer ticks that occurred after booting |
| `uptime` | Output the elapsed time in seconds since booting |
| `fault` | BRK causes a synchronous exception and outputs exception information and stops |
| `shutdown` | Power off QEMU virtual machine via PSCI |
| `reboot` | Reboot QEMU virtual machine via PSCI |

If you enter a command that is not yet implemented, you will receive an error and instructions on how to use `help`.
The parser removes the double and single quotes themselves from the result, and the quoted parts and
Concatenates immediately attached regular characters into the same argument. For example
`echo pre"hello world"post` outputs `prehello worldpost`. Even within quotation marks
You can escape the next character with a backslash. At the end of an unclosed quote or line
backslash prints a clear parse error and does not execute the command.

`sleep` has a busy-wait method that repeatedly reads the timer value and continues to use the CPU.
No. Let the CPU rest with `wfe` and check the elapsed time every time the 100Hz timer IRQ wakes up.
Confirm. UART input that arrives while waiting is stored in the ring buffer.

`fault` is a command for verifying ARM64 exception handlers. If you run
The values `ESR_EL1`, `ELR_EL1`, `FAR_EL1`, and `SPSR_EL1` are output and the system
Stop intentionally. To use it again, close QEMU with `Control-A` or `X`.
Run `make run`.

## Exception handling

At startup, the kernel stores a 2KB aligned ARM64 exception vector table in `VBAR_EL1`.Register. Synchronous exceptions in the current EL, IRQ, FIQ, SError and lower EL are raised.
It has 16 entry points for exceptions.

When an IRQ occurs, general registers and `ELR_EL1` and `SPSR_EL1` are stored on the stack and C
Call the handler. After processing, restore all states and stop with `eret`.
Back to the code. Synchronous exceptions, FIQ, and SError are not yet recovered and exception information is
It reports to the UART and then halts the kernel to prevent further damage.

## Interrupts and system timers

When running, lock the QEMU machine to GICv2, insecure mode, with 1 CPU. Kernel is QEMU
Initializes the GIC Distributor and CPU Interface of `virt` and configures the ARM Generic Timer.
Connect non-secure physical timer (`CNTP_*`) PPI 30. Timer 100 times per second
Increments the tick counter by raising an IRQ.

The PL011 UART uses SPI 1, i.e. GIC INTID 33. Kernel sets UART to 115200 baud,
Initialize to 8-bit, FIFO mode and enable RX·receive-timeout·error interrupt.
The IRQ handler drains the hardware FIFO and stores characters in a ring with an actual capacity of 256 bytes.
Put it in buffer. The shell consumes characters from this buffer, with `wfe` if empty.
Wait. The ISR stores the new character and wakes up the waiting code with `sev`.

When the ring buffer is full, new characters are discarded, but the hardware FIFO continues to be empty so that the IRQ is
Prevent repetition. In `info` command, UART IRQ count, number of bytes received, current
You can check buffer usage, maximum usage, number of drops, and number of hardware errors.

## Memory management and MMU

128MiB RAM of QEMU `virt` is in `0x40000000~0x47ffffff`. kernel image,
Static data, first 4KiB boundary after 64KiB boot stack as linker symbol
It is displayed as `__kernel_end`, and the physical page allocator starts from the next page.
Manage it. New pages are erased with 0 and then returned, and the returned pages are removed from the free-list.
Reuse it first. `NULL`, unaligned address, out-of-range address and double-free
I refuse.

MMU uses 4KiB translation granule and 2MiB block descriptor. present
Since it is identity mapping that keeps the virtual address and physical address the same, even after turning on the MMU,
Kernel, stack, UART, and GIC addresses do not change. GIC and UART
With Device-nGnRnE, RAM is separated with Normal WBWA attribute. One L1 table for TTBR0
12KiB static translation table, one L2 for device and one L2 for RAM.
I use it.

At this stage, only `SCTLR_EL1.M` is turned on to enable address translation. D-cache and
Until I-cache implements safe cache invalidate and per-page execution permissions,
It has been turned off intentionally. You can check the actual register value with the `mmu` command.
There is.

## Cooperative kernel tasks

There are a maximum of 4 task slots, including boot·shell tasks. The new task is physical
Receive a 4KiB stack from the page allocator, and only when calling `task_yield()`.
Switches to the next READY task. x19~x30 and SP according to ARM64 calling convention
It is saved and restored in assembly, and the C structure offset is checked with `_Static_assert`.
Build option `-mgeneral-regs-only` is maintained, so the current FP/SIMD context is not saved.
It doesn't. At the bottom of each stack, a canary is placed and `yield` is used for termination and recovery.Check for damage.

When the task entry function ends, it becomes FINISHED, and in its stack, it becomes FINISHED.
Does not release pages. Only after returning to the shell task, the stack is retrieved and the current
Avoid incorrectly returning the running stack. `taskdemo` is the region of both tasks
It shows whether the state is maintained even after the context switch, and creates, switches, terminates, and
You can see the number of recoveries. All tasks in this stage are in the same address space as EL1.
This is a kernel task that runs, not process isolation.

## Structure

- `src/boot.S`: CPU selection, BSS initialization, stack settings
- `src/kernel.c`: Kernel entry point and shell startup
- `src/uart.c`: PL011 UART IRQ, 256 byte RX ring buffer, input/output
- `src/console.c`: One-line input, editing, arrow key command history
- `src/shell.c`: command/argument parser, command prompt and basic commands
- `src/platform.c`: PSCI-based system shutdown and reboot
- `src/exception_vectors.S`: 16 ARM64 EL1 vectors, IRQ frames and `eret`
- `src/exception.c`: Exception cause and system register UART reporting
- `src/gic.c`: GICv2 initialization and IRQ acknowledge/EOI of QEMU `virt`
- `src/irq.c`: Timer/UART IRQ distribution and IRQ mask release
- `src/timer.c`: ARM Generic Timer initialization and 100Hz tick counter
- `src/mmu.c`: 4KiB translation table, RAM/MMIO identity mapping and MMU activation
- `src/page_alloc.c`: 4KiB physical page bump/free-list allocator
- `src/task.c`: cooperative round-robin scheduler, task creation/termination/stack recovery
- `src/task_switch.S`: ARM64 x19~x30·SP context switch
- `tests/qemu_smoke.py`: QEMU boot and UART shell auto-regressive testing.
- `.github/workflows/test.yml`: Linux automatic testing for push·pull requests
- `linker.ld`: QEMU ARM64 memory placement
- `Makefile`: macOS·Linux tool exploration, cross-compilation, execution and testing

## Current limit

- The GICv2 address of QEMU `virt` is used directly and the Device Tree is not interpreted.
- Runs only one CPU and directly specifies the UART address, 24MHz clock, and GIC INTID.
- Instantaneous input larger than 256 bytes may result in some characters being dropped and logged in statistics.
- The command parser does not support environment variables, command substitution, and wildcard expansion.
- Synchronous exceptions, FIQ, and SError are diagnosed and stopped without recovery.
- MMU uses only identity mapping and 2MiB block, and has R/O·NX·guard permissions per page.
  No. D-cache and I-cache are also not enabled yet.
- The page allocator only manages a fixed 128MiB QEMU RAM and 4KiB units for small objects.
  There is no heap allocator.
- The scheduler is a cooperative type that explicitly `yield`. Timer preemption, priority,
  scheduler-aware There is no UART standby/sleep yet.
- The task stack is currently one 4KiB sheet, and there is canary diagnosis, but there is no MMU guard page or
  There is no separate IRQ stack yet.
- All tasks share the same address space of EL1, and process isolation, file system,
  There are no user programs.

## Next steps

1. Separate `.text`·`.rodata`·stack permissions and add guard page to L3 page table
2. Activate D-cache·I-cache after cache maintenance
3. Change UART·sleep to scheduler-aware standby and add timer preemption
4. Kernel heap and user address space for small objects
5. File system and user programs

## 한국어

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

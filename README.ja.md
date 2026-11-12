# ARM64 QEMU Mini OS

[日本語](README.ja.md) · [English](README.md) · [한국어](README.ko.md)

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

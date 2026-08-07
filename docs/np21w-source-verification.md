# np21w ソース照合による検証記録

日付: 2026-08-07
対象ソース: `external/np21w-source/rev103/np21w-0.86-rev103/np21w-src-main.zip`（rev103）

TSR がハードコードする PC-98 の割り込み・BIOS 作業領域・I/O ポート・キー語を、
AGENTS.md の「推測せず一次資料/エミュレータ実装で検証し docs/ に記録」に従って
np21w rev103 ソースと照合した結果を記録する。

## キーボード BIOS 作業領域（bios/biosmem.h, bios/bios09.c, bios/bios18.c）

| TSR 定数 | 値 | np21w 定義 | 結果 |
|---|---|---|---|
| `PC98_KB_BUFFER_START` | 0x0502 | `MEMW_KB_BUF` = 0x00502 | 一致 |
| `PC98_KB_BUFFER_END` | 0x0522 | `MEMW_KB_SHIFT_TBL` = 0x00522 | 一致 |
| `PC98_KB_HEAD` | 0x0524 | `MEMW_KB_BUF_HEAD` = 0x00524 | 一致 |
| `PC98_KB_TAIL` | 0x0526 | `MEMW_KB_BUF_TAIL` = 0x00526 | 一致 |
| `PC98_KB_COUNT` | 0x0528 | `MEMB_KB_COUNT` = 0x00528 | 一致 |
| raw キー状態 | 0x052a | `MEMX_KB_KY_STS` = 0x0052a | 一致 |
| シフト状態 | 0x053a | `MEMB_SHIFT_STS` = 0x0053a | 一致 |

- Space スキャン 0x34 は `KY_STS` 内 `pos=(0x34>>3)=6, bit=1<<(0x34&7)=0x10` →
  アドレス 0x052a+6=0x0530 の bit4。TSR の `testb $0x10, %es:0x0530` と一致。
- `SHIFT_STS` ビット定義（bios09.c `updateshiftkey`）: bit0=Shift, bit3(0x08)=Graph,
  bit4(0x10)=Ctrl。TSR の修飾マスク（Shift=0x01 / Graph=0x08 / Ctrl=0x10）と一致。
- キー語形式（bios09.c）: 低バイト=文字, 高バイト=スキャンコード。Shift+Space は
  `0x20 | 0x34<<8 = 0x3420`。TSR の hotkey 判定語と一致。

## シリアル I/O ポート（io/serial.c, io/iocore.c）

- 通常モード: `rs232c_bind` の `attachsysoutex(0x0030, 0x0cf1, {o30,o32}, 2)`。
  `attachoutex` 規則 `(i & 0xf1)==0x30` を満たす一致アドレスは 0x30, 0x32, 0x34, 0x36
  の順で func が 0/1/0/1 と交互に割当 →
  **0x30=data, 0x32=status**。TSR の `DATA_PORT=0030 / STATUS_PORT=0032` と一致。
- FIFO モードのみ 0x130/0x132（`#if SUPPORT_RS232C_FIFO` の明示 `attachout(0x130, o30)`
  / `attachout(0x132, o32)`）。TSR コメント「130h/132h は FIFO モード専用」と一致。

## PIT チャンネル 2 とボーレート（io/pit.c, io/serial.c）

- ポート: `attachsysoutex(0x0071, 0x0cf1, pito71, 4)` → 0x71(ch0) 0x73(ch1) 0x75(ch2)
  0x77(ctrl)。TSR の 0x75/0x77 使用と一致。
- ch2 制御レジスタ既定 `0xb6`（`itimer_reset` の `pit.ch[2].ctrl = 0xb6 & 0x3f`）。
  TSR が 0x77 に書く 0xb6 と一致。
- 8251 モード出力（`rs232c_o32`）: TSR の `MODE=0x02` は `dat&0x03==2`（x16）。
  FIFO 速度式 `9600*256 / mul[rawmode&3] / count`、rawmode=0x02→`mul[2]=16` により
  `9600*256/16/8 = 19200` → **カウント 8 = 19200 bps**。TSR 既定 `TIMER_COUNT=0008`
  と AGENTS.md の Speed=19200 に一致。

## コマンド/モード既定値

- 8251 コマンド既定 `0x27`（`rs232c_reset` / `rs232c_bind`）。TSR 既定 `COMMAND=27` と一致。

## Graph キーのホスト側扱い（win9x/winkbd.cpp, keystat.tbl）

- PC-98 キーマトリクス上 Graph はキー番号 `NKEY_GRPH=0x73`、`SHIFT_STS` bit3。
- **ホスト側 Alt が PC-98 GRPH へ変換される**ことが実装で確認できる:
  - `win9x/winkbd.cpp` の `key106[0x12]` = `0x73`（Windows `VK_MENU`=0x12 → PC-98 0x73）。
  - `keystat.tbl` は `0x73` を `GRPH` / `ALT` と定義（`{0x73,"GRPH"},{0x73,"ALT"}`）。
  - `winkbd_keydown()` が `keystat_senddata(0x73)` でゲストへ配信。
- したがって GRAPH+SPACE ホットキーはホスト側では **Alt+Space のキー入力として観測**され、
  ホスト `FocusPc98` がフォーカス復帰待ちの対象とする Shift/Ctrl/Alt（`Keys.Menu`=Alt）
  のいずれかに正しく対応する。

## 結論

TSR / ホストがハードコードする I/O ポート・BIOS 領域・キー語・修飾マスク・
PIT 設定・既定コマンドは、np21w rev103 ソースとすべて一致（推測に基づく定数は無し）。

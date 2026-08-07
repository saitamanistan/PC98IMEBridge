# コードレビュー指摘事項

日付: 2026-08-07
対象: リポジトリ全体（`dos/pc98`, `host/ImeDosBridge`, 共通プロトコル）
検証状況: `make test` 9件成功, `make pc98-tsr` MAP検証（tsr/release）成功

優先度は「重要 / 軽微」で区分しています。

状態: **[対応済み]** = PR `fix/code-review-2026-08-07` で修正済み / **[未対応]** = 残課題

---

## 重要

### 1. INT 28h (idle) / INT 08h (timer) フックは未使用のデッドコード — **[対応済み]**
- 場所: `dos/pc98/tsr_hook.S`（`pc98_idle_hook`, `pc98_timer_hook` とその install/restore 一式）, `dos/pc98/foreground.c` の診断カウンタ
- 内容: 完全に実装されているが、`tsr_main.c` が実際に install するのは `pc98_install_input`（INT18h）と `pc98_install_serial_irq`（INT0Ch）のみで、idle/timer の install はどこからも呼ばれていない。
- 影響: resident 領域を無駄に占有。`foreground.c` の `INT08 HOOK/WORKER` カウンタは常に 0 のまま。
- 対処: 意図的な残骸ならコメントで明示。無ければ削除。
- 対応: フック実装・install/restore・タイマ補助関数・カウンタを削除。`pc98_old_idle_vector` / `pc98_old_timer_vector` は `tools/check_pc98_tsr_map.py` が resident 内存在を必須とするため予約データとして維持しコメントで明記。resident 縮小: `0x18E6..0x23D8` → `0x17CE..0x22BC`。

### 2. `BridgeForm.SendAsync` はシグネチャだけ async — **[対応済み]**
- 場所: `host/ImeDosBridge/BridgeForm.cs:523-532`
- 内容: `lock` 内で同期 `Write/Flush` し `await Task.CompletedTask` を返すだけで、実際にはタスク境界を作らない。
- 影響: `SendText` が `async void` でこの後に `input.Clear()` するため、実質 UI スレッド上で同期的に実行され、書き込みがブロッキングになる。
- 対処: 「送信完了後UIを進める」意図なら現状で可だが、async は見かけだけであることをコメントで明確化もしくは本当に非同期化。
- 対応: `SendAsync` を同期化し未使用オーバーロードを削除。`SendText`/`SendKey`/`SendCloseIme` を同期 `void` に変更。

### 3. `BridgeForm.stream` フィールドのスレッド安全性 — **[対応済み]**
- 場所: `host/ImeDosBridge/BridgeForm.cs:97`, `ServeAsync`, `SendText`/`SendKey`/`SendCloseIme`
- 内容: UI スレッドとリスナースレッド両方から読み、`ServeAsync` が null/差し替えするが `volatile` でもロックでもない。
- 影響: 再接続時に race があり得る。`SendAsync(Stream, ...)` 側は接続済みローカルを使うので実害は小さい。
- 対処: `volatile` 化、もしくは公開をスレッドローカル/接続ハンドル経由に変更。
- 対応: 初回は `stream` を `volatile` 化。レビュー指摘を受け、接続とプロトコル状態(`Stream`/`OpenSequence`/`ImeReady`/`TargetMaxTextBytes`)を単一の `BridgeSession` オブジェクトに集約し、リスナーは**初期化済みセッションを volatile 参照で原子的に公開**、状態遷移と送信は同一セッションのロックで直列化する方式に改めた。

### 4. `FocusPc98` は Shift リリースのみ待つ — **[対応済み]**
- 場所: `host/ImeDosBridge/BridgeForm.cs:248-251`
- 内容: `GetAsyncKeyState(Keys.ShiftKey)` 固定で、Ctrl/Graph ホットキー設定時はそのキーのリリースを待たずにフォーカス移動する。
- 影響: `CTRL+SPACE` / `GRAPH+SPACE` 設定時に「戻ってから再度 IME が開く」競合が起き得る。
- 対処: 設定中のモディファイアに応じて待つキーを切り替える。
- 対応: `HotkeyModifierStillDown()` を追加し、ホスト可視の `Shift`/`Ctrl`/`Alt` モディファイアのリリースを待つよう拡張。一旦 Graph/Alt を外したがレビュー指摘を受け、np21w rev103 では Windows Alt(VK_MENU)=0x12 が PC-98 GRPH(0x73)へ変換される（`win9x/winkbd.cpp key106[0x12]=0x73`, `keystat.tbl 0x73=GRPH/ALT`）ことをソースで確認し、`Keys.Menu`(Alt)を待機対象へ復元。根拠は `docs/np21w-source-verification.md` と `docs/host-bridge.md` に記録。

---

## 軽微

### 5. `Send` / `RemoteEnter` が既定とも Enter で重複し得る — **[未対応]**
- 場所: `host/ImeDosBridge/BridgeKeyBindings.cs:56,58`, `BridgeForm.cs:169-181`
- 内容: 両方の既定が `Keys.Enter`。`OnKeyDown` はテキスト有無で衝突回避しているが、設定で同一キーに割り当てると `RemoteEnter` は常に無視される。
- 対処: 負け側を明示的に無効化、もしくは競合を検証。

### 6. `GetOption` は int 以外の型バリデーションがない — **[未対応]**
- 場所: `host/ImeDosBridge/Program.cs:114-120`
- 内容: `--port` だけ TryParse し、`string` 系オプションは `args[i+1]` をそのまま使う。
- 対処: 拡張時の型チェック追加を検討。

### 7. `pc98_bios_inject` のループと `encode_key` のインデックス進めが暗黙依存 — **[未対応]**
- 場所: `dos/pc98/inject_bios_pc98.c:145-151`
- 内容: `for` ループ内で `pc98_bios_encode_key(&i)` が `i` を進めつつ `++i` も走る。CRLF で 2 バイト進む挙動は `normalized_length` と整合しており正しいが、意図が読み取りにくい。
- 対処: コメントで補足するか、ループ構造を整理。

---

## 良かった点（維持したい事項）

- 共有フレーミング(CRC16)が C / C# で対称実装され、`BitConverter` / `BinaryPrimitives` ともリトルエンディアンで一致。
- TSR のスタックスイッチは各フックで `cli` → resident stack → 復元が一貫し、`pc98_bios_enqueue_word` のクリティカル区間も `pushf/cli` で保護。
- フレーム同期・リセット・バッファ超過・CRC 検証が堅牢で、`make test` がそれを担保。

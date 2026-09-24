# CASIO EX-word XD-B4800 技術仕様・LLM実行予測

更新日: 2026-09-23

メーカー資料、接続個体の実測、公開homebrewからの推定を分離して記録する。CPU型番・クロック・物理RAM総量は未確定であり、推測値を確定仕様として扱わない。

## 仕様表

| 項目 | 値 | 確度・根拠 |
|---|---:|---|
| 製品 | XD-B4800 / DATAPLUS 6 | 実機・メーカー取扱説明書 |
| 発売 | 2011年2月4日 | 公開製品情報 |
| ファームウェア識別 | `gy131,ON,0100 / gy999 / CY168` | 実機をlibexwordで取得 |
| CPU ISA | SuperH、SH-4A互換ターゲット | devkitSH4/libdataplusの実行実績。正確なSoC型番は未確定 |
| CPU識別レジスタ | PVR/PRR実測待ち | 5M版の端末情報画面へ読み取り専用表示を実装 |
| クロック設定 | FRQCR実測待ち（アドレス`0xa4150000`） | libdataplus実装とSH-4A系公開資料。設定変更は行わない |
| FPU | 公開homebrew ABIでは使用しない | ビルドはnofpu。物理FPU有無とは区別 |
| エンディアン | big-endian | devkitSH4 EX-wordターゲットとD01形式 |
| メイン画面 | 5.0型表示、528×320、TFTカラー、タッチ対応 | メーカー資料 |
| サブ画面 | 2.6型、240×96、TFTカラー、タッチ対応 | 製品資料 |
| メインVRAM | RGB565として337,920 bytes | 実働homebrewの `0xAC200000`、528×320×2 |
| 本体ユーザー領域 | 約100MB | 製品資料。RAMではなく不揮発ストレージ |
| 外部記憶 | microSD / microSDHC（当時の動作確認例は最大16GB） | メーカー取扱説明書 |
| USB | `07cf:6101`、`CESG502`、480Mbit/s列挙 | 接続個体で実測。実効速度ではない |
| homebrew用プール設定 | 上限8MiB（`0x8c800000`起点） | CY168向け公開memmgr設定。上限までの確保試験は未実施 |
| 物理RAM総量 | 未確定 | 安全な公開APIがないため断定しない |
| 電源 | 単3形2本、USB給電対応 | メーカー取扱説明書 |
| 公称電池寿命 | アルカリ約70時間（所定の混合利用条件） | メーカー取扱説明書。LLM連続推論時とは条件が異なる |
| 外形・質量 | 148×105.5×17〜19.7mm、約300g（電池込み） | 2011年製品発表資料 |

## EXLLM 5M版の資源見積り

| 資源 | 使用量・演算量 |
|---|---:|
| パラメータ | 5,377,824 |
| 固定小数点モデル | 5,443,105 bytes |
| KVキャッシュ | 884,736 bytes（6層×128×288×K/V×int16） |
| アプリD01 | 約47KB（日本語632字のビットマップを含む） |
| モデル＋KV | 約6.04MiB（このほか実行時バッファを使用） |
| 1生成トークン | 旧1.23M版比で約4.43倍のweight MAC + attention・正規化・softmax |
| コンテキスト | 最大128トークン |

容量には十分な余裕がある。公開版は浮動小数点を排除し、行単位int8重み、Q12 int16活性、整数累積、指数LUT、KVキャッシュを使う。2026-09-24の実機ベンチマークでは、Thinking OFF時のTTFT後生成速度は0.52～0.55 token/秒、TTFTは入力1 tokenあたり約1.58秒だった。詳細は`docs/EXLLM-BENCHMARK-ARTICLE-NOTES.md`を参照。

## モデル大型化の検証

旧版は **1,225,760 parameters** だった。現在は容量上の実用上限候補として5M構成を採用し、**5,377,824 parameters** の端末用モデルをインストール済みである。

|構成|params|model MiB|KV MiB|合計 MiB|対現行演算量|8MiB判定|
|---|---:|---:|---:|---:|---:|---|
|current-1.23M (d=160, L=4, FF=512)|1,225,760|1.20|0.31|1.58|1.00x|余裕あり|
|target-3M (d=256, L=4, FF=832)|3,009,792|2.91|0.50|3.48|2.47x|余裕あり|
|target-4.4M (d=256, L=6, FF=832)|4,387,072|4.24|0.75|5.06|3.61x|余裕あり|
|target-5.4M (d=288, L=6, FF=896, heads=9)|5,377,824|5.19|0.84|6.11|4.43x|余裕あり|
|memory-edge-6.7M (d=320, L=6, FF=1024)|6,712,640|6.47|0.94|7.48|5.54x|要注意|

`tools/estimate_model_capacity.py` で同じ表を再計算できる。5M版はホスト固定小数点検証と実機への読込・登録まで完了した。生成速度と実際の連続メモリ余裕は実機画面で確認する。6.7M構成は8MiBに対する余白が約0.5MiBしかなく、アロケータ丸め、入力・出力バッファ、将来機能の余裕が不足するため非推奨である。

品質向上はパラメータ数だけで決まらない。5M版は未知入力を無理に補完せず言い換えを促す挙動を含め、実機推論が確認された。今後の大型化判断は、端末情報の空き連続メモリと実測生成時間を基準にする。int4化なら格納上限は上がるが、現SH-4Aカーネルでは展開コストが増え、演算量自体は減らない。

## CPU同定の進捗

libdataplusが使うCPG/TMUのレジスタ配置は、SH7723/SH7724およびCasio
SH7305周辺の公開実装と共通点が多い。ただし、アドレス一致だけでSoCをSH7724や
SH7305と断定することはできない。今回、次の値を既存XLLMIの「端末情報」画面へ
読み取り専用で追加した。

- ROMヘッダーのモデル識別子とNORサイズ値（既知アドレス`0x8001ff80`）
- Processor Version Register（PVR、`0xff000030`）
- Product Register（PRR、`0xff000044`）
- Frequency Control Register（FRQCR、`0xa4150000`）

PVR/PRRをRenesas SH-4A資料と照合し、FRQCRのPLL倍率・I/B/Pクロック分周値を
解析することでCPU系統と動作クロックを絞る。基準発振周波数が別途確定するまでは、
FRQCRから絶対MHz値を断定しない。

## Hardware Dump v1 実機結果（2026-09-23）

XD-B4800実機のRTC 1秒とTMU2（Pφ/4）を使った読み取り中心の計測結果。
TMU2は元のレジスタ値を保存し、計測後に復元した。生ダンプは
`evidence/HWINFO-XD-B4800-20260923.txt`に保存した。

| 項目 | 値 | 確度・根拠 |
|---|---:|---|
| CPU / SoC | Casio向けSH7305系 SH-4A | 高。PVR `10300b00`、PRR `00002c00`が公開Casio SH7305実機値と一致 |
| CPUコア/スレッド | 1 / 1 | 高。SH-4A単一コア構成 |
| CPU Iクロック | 24.184 MHz | 実測。Pクロック実測値とFRQCR比から算出 |
| SHwy / Bクロック | 24.184 / 24.184 MHz | 実測比率より算出 |
| Pクロック | 12.092 MHz | RTC 1秒間のTMUカウントから実測 |
| FRQCR | `0f111113` | 実機読み取り。PLL×32、I/SH/B/P分周=3/3/3/6 |
| RAM | 16 MiB級（推定） | 中。C168の既知8 MiBアプリ領域と、7.5 MiB連続確保に成功。物理実装量の直接レジスタは未発見 |
| アプリ連続ヒープ | 7.5 MiB | 実測。256 KiB単位で確保・端点書込・解放後に連続再確保 |
| メモリチャネル | 16-bit単一バスの可能性が高い | 中。SH7305系公開解析と一致。チップ配線の実機確認は未実施 |
| 専用VRAM | 0 MiB | 高。RAM上の337,920-byte RGB565フレームバッファを使用 |
| GPU | 汎用GPUは未識別 | 中。LCDC/DMAはあるがGPU APIは確認できず |
| NPU | なし / 0 TOPS | 高。SH-4A ISAにNPUなし |
| ファームウェアNOR | 32 MiB | ROMヘッダー実機値 `33554432` bytes |
| 内蔵ユーザーストレージ | 100 MiB | USBライブラリープロトコル実測 `104857600` bytes |
| microSD | microSDHC対応、挿入カード約504 MiB | USB実測 `528592384` bytes。カード容量は交換可能 |
| Idle Power | 未測定 | ソフトウェアのみで電流を計測できず、外部電流計が必要 |

## 今後の読み取り専用測定

- Thinking ON時のprompt処理時間、初回token待ち時間、tokens/sをOFF時と分けて記録する。
- 既知アドレス範囲内でアロケータ上限を段階確保する。未知アドレスの総当たりRAMダンプはバスエラーや機密データ混入を避けるため行わない。
- 内蔵ストレージ読込速度とint8 dot productのMAC/sを測る。
- 端末情報画面に表示されたROM model / NOR / PVR / PRR / FRQCRを記録する。
- PVR/PRRをSH-4A資料と照合し、FRQCRの各フィールドをデコードする。

## 参考資料

- CASIO XD-B4800取扱説明書: https://support.casio.jp/pdf/003/XD-B4800_WB.pdf
- libexword: https://github.com/brijohn/libexword
- libdataplus: https://github.com/brijohn/libdataplus
- exword-template: https://github.com/brain-hackers/exword-template
- Gnuboy EX: https://github.com/brijohn/gnuboy-ex
- 2011年製品発表: https://news.mynavi.jp/article/20110112-a134/
- Renesas SH-4A Software Manual: https://www.renesas.com/en/document/mas/sh-4a-software-manual
- Linux SH7723/SH7724 clock definitions: https://codebrowser.dev/linux/linux/arch/sh/kernel/cpu/sh4a/

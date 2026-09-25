# EXLLM for EX-word

日本語 | [English](#english)

EXLLM for EX-wordは、小型日本語LLM「EXLLM 5M」をCASIO EX-word上でオフライン実行するhomebrewアプリです。XD-B4800（DATAPLUS 6）実機で、キーボード入力から回答生成まで確認しています。

## 収録アプリ

- `XLLMI` — 通常利用向け安定版
- `XLMBM` — TTFT・総時間・tokens/sを記録する計測版

電子辞書用`model.q12`は[`ToTo-40417/EXLLM`](https://huggingface.co/ToTo-40417/EXLLM/tree/main/weights)で配布しています。端末ではダウンロードした`model.q12`を本体内蔵領域の`MODELS/model.q12`へ配置します。モデルのソースと学習手順は[`exllm`](https://github.com/ToTo-40417/exllm)を参照してください。

## 実機結果

XD-B4800の5回測定では、Thinking OFF時のTTFT中央値は23.711秒、TTFT後の生成速度は0.52〜0.55 token/秒でした。詳細は[`docs/EXLLM-BENCHMARK-ARTICLE-NOTES.md`](docs/EXLLM-BENCHMARK-ARTICLE-NOTES.md)を参照してください。

Thinking ONでは最大10 tokenのdraftを先に生成し、そのtoken列を`元の質問 + "\n回答:" + draft`という単一USER入力へ直接組み込んで再prefillします。draftはEOSまたは文末記号で早期終了し、最終回答にはThinking OFFと同じ最大48 tokenを確保します。128 tokenのcontextを超える場合は、最終回答、draft、元質問の順で領域を優先し、必要な分だけ元質問の左側を省略します。

## ビルド

devkitSH4と[`libdataplus`](https://github.com/brijohn/libdataplus)が必要です。依存物はこのrepoに同梱していないため、それぞれの配布元から取得してください。

```bash
export DEVKITPRO="$HOME/toolchains/devkitPro"
export DEVKITSH4="$DEVKITPRO/devkitSH4"
export PATH="$DEVKITPRO/tools/bin:$DEVKITSH4/bin:$PATH"
make -C apps/exllm
make -C apps/exllm-benchmark
```

ビルド済みD01ではなくソースを公開しています。必要な依存物は上流repoから直接取得してビルドしてください。インストール中はUSBを切断しないでください。

## ライセンス

GPL-2.0。Gnuboy EX由来の最小libcと[`brain-hackers/exword-template`](https://github.com/brain-hackers/exword-template)を基礎にしています。モデルはApache-2.0で別配布です。ゲームROM、セーブデータ、CASIO firmware、端末認証情報は含みません。再配布や派生版の公開はライセンスに従って自由に行えますが、活用状況を把握するため、公開時に作者へ一報いただけると幸いです（連絡は利用条件ではありません）。

## 関連プロジェクト

- [`EXLLM`](https://github.com/ToTo-40417/exllm) — 学習・評価コードと参照ランタイム
- [Hugging Faceモデル・配布用重み](https://huggingface.co/ToTo-40417/EXLLM)
- [`exword-hardware-dump`](https://github.com/ToTo-40417/exword-hardware-dump) — EX-word実機情報の取得
- [`exword-gnuboy-save-importer`](https://github.com/ToTo-40417/exword-gnuboy-save-importer) — Gnuboyセーブデータ転送

## English

EXLLM for EX-word runs the EXLLM 5M Japanese language model fully offline on CASIO EX-word hardware. Keyboard input and token-by-token generation have been verified on an XD-B4800 (DATAPLUS 6).

This repository contains the stable `XLLMI` app and the separate `XLMBM` benchmark build. The device-ready [`model.q12`](https://huggingface.co/ToTo-40417/EXLLM/tree/main/weights) is distributed on Hugging Face; place it at `MODELS/model.q12` in the device's internal storage. Model source and training instructions are available in [`exllm`](https://github.com/ToTo-40417/exllm). On the physical device, median TTFT was 23.711 seconds and post-TTFT generation was 0.52–0.55 token/s with Thinking disabled.

With Thinking enabled, the runtime generates a draft of up to 10 tokens and reuses those exact token IDs in a second single-turn USER prompt: `original question + "\n回答:" + draft`. EOS and sentence-ending punctuation may stop the draft early. The final pass keeps the same 48-token generation limit as normal mode. Within the fixed 128-token context, final-answer space takes priority, followed by the draft; the left side of an overlong original question is trimmed only when required.

devkitSH4 and [`libdataplus`](https://github.com/brijohn/libdataplus) are required but not bundled. Obtain each dependency directly from its upstream repository and build the apps from source. Do not disconnect USB while installing.

GPL-2.0 source. The Apache-2.0 model, game ROMs, save data, CASIO firmware, and device authentication data are not included. Redistribution and derivative releases are welcome under the license. If you publish one, a brief note to the author would be appreciated, but is not a condition of use.

Related projects: [`EXLLM`](https://github.com/ToTo-40417/exllm), [`EXLLM on Hugging Face`](https://huggingface.co/ToTo-40417/EXLLM), [`exword-hardware-dump`](https://github.com/ToTo-40417/exword-hardware-dump), and [`exword-gnuboy-save-importer`](https://github.com/ToTo-40417/exword-gnuboy-save-importer).

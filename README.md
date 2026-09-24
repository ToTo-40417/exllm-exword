# EXLLM for EX-word

日本語 | [English](#english)

EXLLM for EX-wordは、小型日本語LLM「EXLLM 5M」をCASIO EX-word上でオフライン実行するhomebrewアプリです。XD-B4800（DATAPLUS 6）実機で、キーボード入力から回答生成まで確認しています。

## 収録アプリ

- `XLLMI` — 通常利用向け安定版
- `XLMBM` — TTFT・総時間・tokens/sを記録する計測版

モデルは別配布です。`model.q12`を[`ToTo-40417/EXLLM`](https://huggingface.co/ToTo-40417/EXLLM)から取得し、本体内蔵領域の`MODELS/model.q12`へ配置します。

## 実機結果

XD-B4800の5回測定では、Thinking OFF時のTTFT中央値は23.711秒、TTFT後の生成速度は0.52〜0.55 token/秒でした。詳細は[`docs/EXLLM-BENCHMARK-ARTICLE-NOTES.md`](docs/EXLLM-BENCHMARK-ARTICLE-NOTES.md)を参照してください。

## ビルド

devkitSH4とlibdataplusが必要です。依存物はこのrepoに同梱しません。

```bash
export DEVKITPRO="$HOME/toolchains/devkitPro"
export DEVKITSH4="$DEVKITPRO/devkitSH4"
export PATH="$DEVKITPRO/tools/bin:$DEVKITSH4/bin:$PATH"
make -C apps/exllm
make -C apps/exllm-benchmark
```

`libdataplus`全体の再配布条件が明示されるまで、このrepoはソースを公開し、静的リンク済みD01は配布しません。インストール中はUSBを切断しないでください。

## ライセンス

GPL-2.0。Gnuboy EX由来の最小libcと[`brain-hackers/exword-template`](https://github.com/brain-hackers/exword-template)を基礎にしています。モデルはApache-2.0で別配布です。ゲームROM、セーブデータ、CASIO firmware、端末認証情報は含みません。

## English

EXLLM for EX-word runs the EXLLM 5M Japanese language model fully offline on CASIO EX-word hardware. Keyboard input and token-by-token generation have been verified on an XD-B4800 (DATAPLUS 6).

This repository contains the stable `XLLMI` app and the separate `XLMBM` benchmark build. The model is distributed separately from [`ToTo-40417/EXLLM`](https://huggingface.co/ToTo-40417/EXLLM). On the physical device, median TTFT was 23.711 seconds and post-TTFT generation was 0.52–0.55 token/s with Thinking disabled.

devkitSH4 and libdataplus are required but not bundled. Prebuilt D01 files are withheld until libdataplus redistribution terms are explicit. Do not disconnect USB while installing.

GPL-2.0 source. The Apache-2.0 model, game ROMs, save data, CASIO firmware, and device authentication data are not included.

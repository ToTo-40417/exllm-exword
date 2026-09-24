# Benchmark app / 計測版

`XLMBM` is a separate performance-instrumented build. It records prompt length, output tokens, TTFT, total latency, and generation speed to `XLMBM/_USER/PERFLOG.TSV`. It does not replace the stable `XLLMI` app.

`XLMBM`は安定版と共存する計測専用ビルドです。入力・出力token数、TTFT、総時間、生成速度を`XLMBM/_USER/PERFLOG.TSV`へ追記します。

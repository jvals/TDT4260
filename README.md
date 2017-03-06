# TDT4260
Prefetcher algorithms for the M5 simulator

## Running the Simulator

```bash
cd framework
./compile
./m5/build/ALPHA_SE/m5.opt m5/configs/example/se.py --detailed \
--caches --l2cache --l2size=1MB --prefetcher=policy=proxy \
--prefetcher=on_access=True
```

## Running SPEC CPU2000

```bash
./test_prefetcher.py
```

# TODO

## Macros
- [ ] ~~ILP_WHILE/ILP_DO_WHILE~~ More research needed

## Testing
- [ ] Add more extreme unit tests for ILP_FOR (edge cases, stress tests)
- [x] ~~are asm test serving any real purpose?~~ Removed - low value tests
- [ ] might want to overhaul benchmarks
- [x] Make UBSan and ASan more aggressive

## Experimental
- [ ] Experiment with DSL for calculating ILP unroll value for N

## CPU Profiles
- [x] Add compare/branch instruction info to cpu profile headers (FCMP, VUCOMISS latency/throughput)
- [x] Document Search loop N derivation from compare instruction characteristics

## Misc
- [ ] ARM64 MCA on Godbolt still showing llvm-mc errors (may be Godbolt infra issue)

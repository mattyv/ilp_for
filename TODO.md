# TODO

## Macros
- [ ] ~~ILP_WHILE/ILP_DO_WHILE~~ More research needed

- [ ] sbo size dependant on hardware register width 

## functions 
- [ ] ~~change api to follow std conventions~~. functions removed

## Testing
- [x] ~~Add smoke tests for untested macro variants~~ Added tests for ILP_FOR_RANGE_T, ILP_FOR_T_AUTO, ILP_FOR_RANGE_T_AUTO
- [x] ~~are asm test serving any real purpose?~~ Removed - low value tests
- [ ] might want to overhaul benchmarks
- [x] Make UBSan and ASan more aggressive

## Experimental
- [X] ~~Experiment with DSL for calculating ILP unroll value for N.~~ Done with clang-tidy

## CPU Profiles
- [x] Add compare/branch instruction info to cpu profile headers (FCMP, VUCOMISS latency/throughput)
- [x] Document Search loop N derivation from compare instruction characteristics

## Misc
- [x] ~~ARM64 MCA on Godbolt~~ Fixed invalid compiler IDs in tuner (no ARM64 Clang on Godbolt, use GCC)

## readme
- [x] ~~update SBO to discuss how UB is handled~~ Added compile-time enforcement and std::launder info

## LLM
- [ ] add chroma db or some vector db that can be committed to git and add's help to llm's.
CXX = /opt/homebrew/bin/g++-15
CXXFLAGS = -std=c++23 -O3 -Wall -Wextra -I..

# Handrolled sources
HANDROLLED_SRCS = $(wildcard asm_compare/handrolled/*.cpp)
HANDROLLED_OBJS = $(HANDROLLED_SRCS:.cpp=.o)

# ILP sources
ILP_SRCS = $(wildcard asm_compare/ilp/*.cpp)
ILP_OBJS = $(ILP_SRCS:.cpp=.o)

# Simple sources (baseline)
SIMPLE_SRCS = $(wildcard asm_compare/simple/*.cpp)
SIMPLE_OBJS = $(SIMPLE_SRCS:.cpp=.o)

# Std sources (standard library)
STD_SRCS = $(wildcard asm_compare/std/*.cpp)
STD_OBJS = $(STD_SRCS:.cpp=.o)

# Test sources
TEST_SRCS = $(wildcard correctness/*.cpp)

.PHONY: all clean test test-asm handrolled ilp simple std asm

all: handrolled ilp simple std

handrolled: $(HANDROLLED_OBJS)
	@echo "Handrolled objects built successfully"

ilp: $(ILP_OBJS)
	@echo "ILP objects built successfully"

simple: $(SIMPLE_OBJS)
	@echo "Simple objects built successfully"

std: $(STD_OBJS)
	@echo "Std objects built successfully"

# Build and run all tests (includes asm generation)
test: handrolled_asm ilp_asm correctness/test_runner
	./correctness/test_runner

# Build and run assembly size tests only
test-asm: handrolled_asm ilp_asm correctness/test_runner
	./correctness/test_runner "[asm]"

correctness/test_runner: $(HANDROLLED_OBJS) $(ILP_OBJS) $(TEST_SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $(HANDROLLED_OBJS) $(ILP_OBJS) $(TEST_SRCS)

# Compile individual files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Assembly comparison
asm: handrolled_asm ilp_asm simple_asm std_asm
	@echo "Assembly files generated in asm_compare/"

handrolled_asm:
	@for f in $(HANDROLLED_SRCS); do \
		$(CXX) $(CXXFLAGS) -S $$f -o $${f%.cpp}.s; \
	done

ilp_asm:
	@for f in $(ILP_SRCS); do \
		$(CXX) $(CXXFLAGS) -S $$f -o $${f%.cpp}.s; \
	done

simple_asm:
	@for f in $(SIMPLE_SRCS); do \
		$(CXX) $(CXXFLAGS) -S $$f -o $${f%.cpp}.s; \
	done

std_asm:
	@for f in $(STD_SRCS); do \
		$(CXX) $(CXXFLAGS) -S $$f -o $${f%.cpp}.s; \
	done

clean:
	rm -f $(HANDROLLED_OBJS) $(ILP_OBJS) $(SIMPLE_OBJS) $(STD_OBJS)
	rm -f asm_compare/handrolled/*.s asm_compare/ilp/*.s asm_compare/simple/*.s asm_compare/std/*.s
	rm -f correctness/test_runner

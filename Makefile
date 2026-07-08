# Convenience wrapper around the CMake build and the course grader.
# (The upstream course targets used flexc++/bisonc++ in Docker; this repo uses
#  standard flex/bison, so the targets below drive the plain cmake build.)

BUILD := build

.PHONY: build clean gradelab1 gradelab2 gradelab3 gradelab4 gradelab5 gradeall \
        test_slp test_lex test_parse test_semant tiger-compiler

build:
	mkdir -p $(BUILD) && cd $(BUILD) && cmake .. && $(MAKE) -j

test_slp test_lex test_parse test_semant tiger-compiler:
	mkdir -p $(BUILD) && cd $(BUILD) && cmake .. >/dev/null && $(MAKE) $@ -j

gradelab1: ; bash scripts/grade.sh lab1
gradelab2: ; bash scripts/grade.sh lab2
gradelab3: ; bash scripts/grade.sh lab3
gradelab4: ; bash scripts/grade.sh lab4
gradelab5: ; bash scripts/grade.sh lab5
gradeall:  ; bash scripts/grade.sh all

clean:
	rm -rf $(BUILD)
	rm -f testdata/**/*.tig.s testdata/**/*.tig.ll

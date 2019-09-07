INCLUDES=-Iparsing/inc -Iutils/inc
OBJECTS =obj
BINARIES=bin
C-FLAGS =-Wall

UTILS  =$(OBJECTS)/error.o $(OBJECTS)/struct.o $(OBJECTS)/array.o
PARSING=$(OBJECTS)/symbol.o $(OBJECTS)/generic_parser.o $(OBJECTS)/tracked_file.o $(OBJECTS)/bnf_parser.o

test: parse-test bnf-test array-test

%-test: $(BINARIES)/%_test
	@./$<

$(BINARIES)/%: $(OBJECTS)/%.o $(UTILS) $(PARSING)
	@gcc $(C-FLAGS) $^ -o $@

$(OBJECTS)/%.o: parsing/src/%.c
	@gcc $(C-FLAGS) $(INCLUDES) -c $< -o $@

$(OBJECTS)/%.o: utils/src/%.c
	@gcc $(C-FLAGS) $(INCLUDES) -c $< -o $@

$(OBJECTS)/%.o: test/%.c
	@gcc $(C-FLAGS) $(INCLUDES) -c $< -o $@

clean-objects:
	@rm -f $(OBJECTS)/*

clean-binaries:
	@rm -f $(BINARIES)/*

clean: clean-objects clean-binaries
	@find . -name "*~" -delete
	@find . -name "#*#" -delete

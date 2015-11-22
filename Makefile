EXEC := battery
ifeq ($(DEBUG),YES)
type := -g
else
type := -O3
endif
CXX ?= g++

$(EXEC): $(EXEC).cpp
	$(CXX) -std=c++14 $(type) $< -o $@

.PHONY: install
install:
	cp $(EXEC) ~/.i3/custom/

PHONY: clean
clean:
	rm -f -- $(EXEC)

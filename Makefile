EXEC ?= battery
PREFIX ?= ~/.config/i3/custom/
CXX ?= g++

ifeq ($(DEBUG),YES)
type := -g
else
type := -O3
endif

$(EXEC): $(EXEC).cpp
	$(CXX) -std=c++14 $(type) $< -o $@

$(PREFIX): 
	mkdir -p $@

.PHONY: install
install: $(EXEC) | $(PREFIX)
	cp $(EXEC) $(PREFIX)

PHONY: clean
clean:
	rm -f -- $(EXEC)

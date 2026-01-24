CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall

all: engine sample_bot

engine: engine.cpp
	$(CXX) $(CXXFLAGS) -o engine engine.cpp

sample_bot: sample_bot.cpp
	$(CXX) $(CXXFLAGS) -o sample_bot sample_bot.cpp

run: all
	./engine ./sample_bot ./sample_bot ./sample_bot ./sample_bot ./sample_bot

clean:
	rm -f engine sample_bot

.PHONY: all clean run

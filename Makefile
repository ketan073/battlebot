CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall

all: engine sample_bot tournament viewer

engine: engine.cpp
	$(CXX) $(CXXFLAGS) -o engine engine.cpp

sample_bot: sample_bot.cpp
	$(CXX) $(CXXFLAGS) -o sample_bot sample_bot.cpp

tournament: tournament.cpp
	$(CXX) $(CXXFLAGS) -o tournament tournament.cpp

viewer:
	@mkdir -p viewer/build
	@cd viewer/build && cmake .. && cmake --build . -j$$(nproc)

run-engine: all
	./engine ./sample_bot ./sample_bot ./sample_bot ./sample_bot ./sample_bot

run-tournament: all
	./tournament ./sample_bot ./sample_bot ./sample_bot ./sample_bot ./sample_bot

run-viewer: viewer
	./viewer/build/viewer

clean:
	rm -f engine sample_bot tournament

distclean: clean
	rm -rf viewer/build
	rm -rf replays/
	rm -f imgui.ini

.PHONY: all clean run viewer run-viewer distclean
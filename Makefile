CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall

all: engine sample_bot viewer

engine: engine.cpp
	$(CXX) $(CXXFLAGS) -o engine engine.cpp

sample_bot: sample_bot.cpp
	$(CXX) $(CXXFLAGS) -o sample_bot sample_bot.cpp

viewer:
	@mkdir -p viewer/build
	@cd viewer/build && cmake .. && cmake --build . -j$$(nproc)

run: all
	./engine ./sample_bot ./sample_bot ./sample_bot ./sample_bot ./sample_bot

run-viewer: viewer
	./viewer/build/viewer

clean:
	rm -f engine sample_bot

distclean: clean
	rm -rf viewer/build
	rm -rf replays/
	rm -f imgui.ini

.PHONY: all clean run viewer run-viewer distclean
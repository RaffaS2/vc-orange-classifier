CXX     = g++
CXXFLAGS = $(shell pkg-config --cflags opencv4)
LIBS     = $(shell pkg-config --libs opencv4)

vctrack: source.o functions.o
	$(CXX) source.o functions.o -o vctrack $(LIBS)

source.o: source.cpp functions.h
	$(CXX) $(CXXFLAGS) -c source.cpp

functions.o: functions.cpp functions.h
	$(CXX) $(CXXFLAGS) -c functions.cpp

# "make clean" limpa os .o e o executável
clean:
	rm -f *.o vctrack
FLAGS=-Wall -Wextra -Wno-unused-label -Wno-sign-compare -g

.PHONY: all


main: src/main.cpp build/utils.o
	g++ $(FLAGS) -o main src/main.cpp build/utils.o

build/utils.o: src/utils.cpp
	mkdir -p build
	g++ -c $(FLAGS) -o build/utils.o src/utils.cpp

clean:
	rm -rf build
	rm main

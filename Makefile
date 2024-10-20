FLAGS=-Wall -Wextra -Wno-unused-label -Wno-sign-compare

.PHONY: all

all: main
	cp main 0/
	cp main 1/
	cp main 2/
	cp main 3/
	cp main 4/

main: src/main.cpp build/parse_files.o build/utils.o
	g++ $(FLAGS) -o main src/main.cpp build/parse_files.o build/utils.o

build/parse_files.o: src/parse_files.cpp
	mkdir -p build
	g++ -c $(FLAGS) -o build/parse_files.o src/parse_files.cpp

build/utils.o: src/utils.cpp
	mkdir -p build
	g++ -c $(FLAGS) -o build/utils.o src/utils.cpp

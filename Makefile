CC=mpicc

FLAGS=-O3 -Wno-unused-result
#DEBUG=-DDEBUG
RESULT=-DRESULT

all: gol

gol: gol.c
	$(CC) $(DEBUG) $(RESULT) $(FLAGS) gol.c -o gol

clean:
	rm -rf gol

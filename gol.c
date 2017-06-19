/*
* The Game of Life
*
* a cell is born, if it has exactly three neighbours
* a cell dies of loneliness, if it has less than two neighbours
* a cell dies of overcrowding, if it has more than three neighbours
* a cell survives to the next generation, if it does not die of loneliness
* or overcrowding
*
* In this version, a 2D array of ints is used.  A 1 cell is on, a 0 cell is off.
* The game plays a number of steps (given by the input), printing to the screen each time.  'x' printed
* means on, space means off.
*
*/
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

typedef unsigned char cell_t;

cell_t ** allocate_board (int size);
void free_board (cell_t ** board, int size);
int adjacent_to (cell_t ** board, int size, int i, int j);
void play (cell_t ** board, cell_t ** newboard, int size, int start, int end);
void print (cell_t ** board, int size);
void read_file (FILE * f, cell_t ** board, int size);


int main (int argc, char *argv[]) {

  int rank, num_processes;

  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &num_processes);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  if (rank == 0) {    /* Processo mestre */
    printf("ALOOOOOOOOOO\n");
    // Lendo arquivo, alocando memória
    int size, steps;
    FILE *f;
    f = stdin;
    fscanf(f,"%d %d", &size, &steps);
    cell_t ** prev = allocate_board (size);
    read_file (f, prev,size);
    fclose(f);
    cell_t ** next = allocate_board (size);
    cell_t ** tmp;

    #ifdef DEBUG
    printf("Initial:\n");
    print(prev,size);
    #endif

    // Dividindo trabalho
    // num_processes - 1 porque o primeiro é o mestre
    int lines = size/(num_processes-1); 
    int reminder = size%(num_processes-1);

    // Broadcast do numero de linhas
    MPI_Bcast(&lines, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // Último recebe quantidade diferente (linhas + resto)
    int last = lines + reminder;
    MPI_Send(&last, 1, MPI_INT, (num_processes-1), 0, MPI_COMM_WORLD);

    // Enviando trabalho para escravos

    #ifdef RESULT
    printf("Final:\n");
    print (prev,size);
    #endif

    free_board(prev,size);
    free_board(next,size);
  }

  /*for (int i=0; i<steps; i++) {
    play (prev,next,size);
    #ifdef DEBUG
    printf("%d ----------\n", i + 1);
    print (next,size);
    #endif
    tmp = next;
    next = prev;
    prev = tmp;
  }*/

  MPI_Finalize();
}

cell_t ** allocate_board (int size) {
  cell_t ** board = (cell_t **) malloc(sizeof(cell_t*)*size);
  int i;
  for (i=0; i<size; i++)
  board[i] = (cell_t *) malloc(sizeof(cell_t)*size);
  return board;
}

void free_board (cell_t ** board, int size) {
  int     i;
  for (i=0; i<size; i++)
  free(board[i]);
  free(board);
}


/* return the number of on cells adjacent to the i,j cell */
int adjacent_to (cell_t ** board, int size, int i, int j) {
  int k, l, count=0;

  int sk = (i>0) ? i-1 : i;
  int ek = (i+1 < size) ? i+1 : i;
  int sl = (j>0) ? j-1 : j;
  int el = (j+1 < size) ? j+1 : j;

  for (k=sk; k<=ek; k++)
  for (l=sl; l<=el; l++)
  count+=board[k][l];
  count-=board[i][j];

  return count;
}

void play (cell_t ** board, cell_t ** newboard, int size, int start, int end) {
  int i, j, a;
  /* for each cell, apply the rules of Life */
  #pragma omp parallel for collapse (2)
  for (i=0; i<size; i++) {
    for (j=0; j<size; j++) {
      a = adjacent_to (board, size, i, j);
      if (a == 2) newboard[i][j] = board[i][j];
      if (a == 3) newboard[i][j] = 1;
      if (a < 2) newboard[i][j] = 0;
      if (a > 3) newboard[i][j] = 0;
    }
  }
}

/* print the life board */
void print (cell_t ** board, int size) {
  int i, j;
  /* for each row */
  for (j=0; j<size; j++) {
    /* print each column position... */
    for (i=0; i<size; i++)
    printf ("%c", board[i][j] ? 'x' : ' ');
    /* followed by a carriage return */
    printf ("\n");
  }
}

/* read a file into the life board */
void read_file (FILE * f, cell_t ** board, int size) {
  int i, j;
  char  *s = (char *) malloc(size+10);

  /* read the first new line (it will be ignored) */
  fgets (s, size+10,f);

  /* read the life board */
  for (j=0; j<size; j++) {
    /* get a string */
    fgets (s, size+10,f);
    /* copy the string to the life board */
    for (i=0; i<size; i++)
    board[i][j] = s[i] == 'x';

  }
}

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

cell_t ** allocate_board (int lines, int rows);
void free_board (cell_t ** board, int size);
int adjacent_to (cell_t ** board, int size, int i, int j);
void play (cell_t ** board, cell_t ** newboard, int size, int lines, int start, int end);
void print (cell_t ** board, int size);
void read_file (FILE * f, cell_t ** board, int size);

void master(int rank) {
  /*************      Processo mestre   *************/

  /*  Lendo arquivo, alocando memória
   */ 
  int size, steps;
  FILE *f;
  f = stdin;
  fscanf(f,"%d %d", &size, &steps);
  cell_t ** prev = allocate_board (size, size);
  read_file (f, prev,size);
  fclose(f);
  cell_t ** next = allocate_board (size, size);

  #ifdef DEBUG
  printf("Initial:\n");
  print(prev,size);
  #endif

  /*  Devido a divisão de trabalho escolhida, utiliza-se no máximo
   *  "size" processos escravos, isto é, o número máximo de processos
   *  escravos é o número de linhas da matriz.
   *  Além dos escravos, sempre terá um processo mestre.
   */
  int num_proc;
  MPI_Comm_size(MPI_COMM_WORLD, &num_proc);
  if (num_proc > size + 1) {
    num_proc = size + 1;
  }

  /*  Dividindo linhas por processos para broadcast
   *  num_proc - 1 porque o primeiro é o mestre
   */ 
  int lines = size/(num_proc-1); 
  int reminder = size%(num_proc-1);

  MPI_Bcast(&lines, 1, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(&steps, 1, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(&size, 1, MPI_INT, 0, MPI_COMM_WORLD);

  // Último recebe quantidade diferente (linhas + resto)
  int last = lines + reminder;
  MPI_Send(&last, 1, MPI_INT, (num_proc-1), 0, MPI_COMM_WORLD);

  MPI_Status st;
  MPI_Request* requests = (MPI_Request*) malloc(sizeof(MPI_Request) * num_proc-1);

  /*  Enviando e recebendo trabalho para/de escravos conforme steps.
   *  Cada processo recebe suas linhas + a linha anterior e a seguinte
   *  para poder utilizar o método adjacent_to()
   */
  for (int j = 0; j < steps; ++j) {
    // Enviando board
    MPI_Isend(prev, (lines+1)*size, MPI_UNSIGNED_CHAR, 1, 0, MPI_COMM_WORLD, &(requests[0]));

    int i;
    for (i = 2; i < num_proc - 1; ++i) {
      MPI_Isend(prev+((i-2)*lines*size), (lines+2)*size, MPI_UNSIGNED_CHAR, i, 0, MPI_COMM_WORLD, &(requests[i-1]));
    }

    MPI_Isend(prev+(((i*lines)-1) * size), (last+1)*size, MPI_UNSIGNED_CHAR, num_proc-1, 0, MPI_COMM_WORLD, &(requests[i]));

    // num_proc-2 ou num_proc-1 ??
    MPI_Waitall(num_proc-2, requests, MPI_STATUSES_IGNORE);

    // Recebendo board novo
    MPI_Irecv(prev, (lines+1)*size, MPI_UNSIGNED_CHAR, 1, 0, MPI_COMM_WORLD, &(requests[0]));

    for (i = 2; i < num_proc - 1; ++i) {
      MPI_Irecv(prev+((i-2)*lines*size), (lines+2)*size, MPI_UNSIGNED_CHAR, i, 0, MPI_COMM_WORLD, &(requests[i-1]));
    }

    MPI_Irecv(prev+(((i*lines)-1) * size), (last+1)*size, MPI_UNSIGNED_CHAR, num_proc-1, 0, MPI_COMM_WORLD, &(requests[i]));

    MPI_Waitall(num_proc-2,requests, MPI_STATUSES_IGNORE);

    #ifdef DEBUG
    printf("%d ----------\n", i + 1);
    print (prev,size);
    #endif
  }

  #ifdef RESULT
  printf("Final:\n");
  print (prev,size);
  #endif

  free_board(prev,size);
  free_board(next,size);  
}

void slave(int rank) {
  /*************    Processos escravos     *************/

  // Recebendo número de linhas, passos e tamanho
  int lines, size, steps, num_proc, count;
  MPI_Status st;

  MPI_Comm_size(MPI_COMM_WORLD, &num_proc);

  MPI_Bcast(&lines, 1, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(&steps, 1, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(&size, 1, MPI_INT, 0, MPI_COMM_WORLD);
  
  /****************    Último processo     ****************/
  if (rank == num_proc-1) {
    MPI_Recv(&lines, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, NULL);

    cell_t ** prev = allocate_board(lines+1, size);
    cell_t ** next = allocate_board(lines+1, size);

    for (int i = 0; i < steps; ++i) {
      MPI_Recv(prev, (lines+1)*size, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD, &st);
      
      MPI_Get_count(&st, MPI_UNSIGNED_CHAR, &count);

      play(prev, next, size, lines+1, 1, lines);

      MPI_Send(next, (lines+1)*size, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD);
    }

    free(next);
    free(prev);

  } else if (rank == 1) {
    /****************    Primeiro processo     ****************/
    cell_t ** prev = allocate_board(lines+1, size);
    cell_t ** next = allocate_board(lines+1, size);

    for (int i = 0; i < steps; ++i) {
      MPI_Recv(prev, (lines+1)*size, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD, &st);
      
      MPI_Get_count(&st, MPI_UNSIGNED_CHAR, &count);

      play(prev, next, size, lines+2, 1, lines);

      MPI_Send(next, (lines+2)*size, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD);
    }

    free(next);
    free(prev);

  } else {
    /****************     Processos do meio      ****************/
    cell_t ** prev = allocate_board(lines+2, size);
    cell_t ** next = allocate_board(lines+2, size);

    for (int i = 0; i < steps; ++i) {
      MPI_Recv(prev, (lines+2)*size, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD, &st);
      
      MPI_Get_count(&st, MPI_UNSIGNED_CHAR, &count);

      play(prev, next, size, lines+2, 1, lines);

      MPI_Send(next, (lines+2)*size, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD);
    }

    free(next);
    free(prev);
  } 
}

int main (int argc, char *argv[]) {

  int rank;

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  if (rank == 0) {
    master(rank);
  } else {
    slave(rank);
  }

  MPI_Finalize();
}

/* return the number of on cells adjacent to the i,j cell */
int adjacent_to (cell_t ** board, int size, int i, int j) {
  int k, l, count=0;

  int sk = (i>0) ? i-1 : i;
  int ek = (i+1 < size) ? i+1 : i;
  int sl = (j>0) ? j-1 : j;
  int el = (j+1 < size) ? j+1 : j;

  for (k=sk; k<=ek; k++) {
    for (l=sl; l<=el; l++) {
      count+=board[k][l];
    }
  }
  count-=board[i][j];

  return count;
}

void play (cell_t ** board, cell_t ** newboard, int size, int lines, int start, int end) {
  int i, j, a;
  /* for each cell, apply the rules of Life */
  for (i=start; i<end; i++) {
    for (j=0; j<size; j++) {
      a = adjacent_to (board, size, i, j);
      if (a == 2) newboard[i][j] = board[i][j];
      if (a == 3) newboard[i][j] = 1;
      if (a < 2) newboard[i][j] = 0;
      if (a > 3) newboard[i][j] = 0;
    }
  }
}

cell_t ** allocate_board (int rows, int cols) {
  cell_t ** board = (cell_t **) malloc(sizeof(cell_t*)*rows);
  int i;
  for (i=0; i<rows; i++)
    board[i] = (cell_t *) malloc(sizeof(cell_t)*cols);
  return board;
}

void free_board (cell_t ** board, int size) {
  int     i;
  for (i=0; i<size; i++)
  free(board[i]);
  free(board);
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

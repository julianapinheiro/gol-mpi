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

int adjacent_to (cell_t * board, int size, int lines, int i, int j);
void play (cell_t * board, cell_t * newboard, int size, int lines, int start, int end);
void print (cell_t * board, int size, int lines);
void read_file (FILE * f, cell_t * board, int size);

void master(int rank) {
  /*************      Processo mestre   *************/

  /*  Lendo arquivo, alocando memória
   */ 
  int size, steps;
  FILE *f;
  f = stdin;
  fscanf(f,"%d %d", &size, &steps);
  cell_t * prev = (cell_t *) malloc(sizeof(cell_t)*size*size);
  read_file (f, prev,size);
  fclose(f);

  #ifdef DEBUG
  printf("Initial:\n");
  print(prev,size, size);
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

  int info[3] = {lines, size, steps};
  MPI_Bcast(info, 3, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Barrier(MPI_COMM_WORLD);

  // Último recebe quantidade diferente (linhas + resto)
  int last = lines + reminder;
  MPI_Send(&last, 1, MPI_INT, (num_proc-1), 0, MPI_COMM_WORLD);

  int flag;
  MPI_Status st;

  /*  Enviando e recebendo trabalho para/de escravos conforme steps.
   *  Cada processo recebe suas linhas + a linha anterior e a seguinte
   *  para poder utilizar o método adjacent_to()
   */
  for (int j = 0; j < steps; ++j) {
    // Enviando board
    MPI_Send(prev, (lines+1)*size, MPI_UNSIGNED_CHAR, 1, 0, MPI_COMM_WORLD);

    int i;
    for (i =1; i < num_proc - 2; ++i) {
      MPI_Send((prev+((i)*lines-1)*size), (lines+2)*size, MPI_UNSIGNED_CHAR, i+1, 0, MPI_COMM_WORLD);
    }
    MPI_Send(prev+((((i)*lines)-1)*size), (last+1)*size, MPI_UNSIGNED_CHAR, num_proc-1, 0, MPI_COMM_WORLD);
    
    // Recebendo board novo
    MPI_Recv(prev, lines*size, MPI_UNSIGNED_CHAR, 1, 0, MPI_COMM_WORLD, &st);

    for (i = 1; i < num_proc - 2; ++i) {
      MPI_Recv(prev+(i*lines*size), lines*size, MPI_UNSIGNED_CHAR, i+1, 0, MPI_COMM_WORLD, &st);
    }

    MPI_Recv(prev+(i*lines*size), last*size, MPI_UNSIGNED_CHAR, num_proc-1, 0, MPI_COMM_WORLD, &st);

    MPI_Barrier(MPI_COMM_WORLD);

    #ifdef DEBUG
    printf("%d ----------\n", j);
    print(prev,size, size);
    #endif
  }

  #ifdef RESULT
  printf("Final:\n");
  print(prev,size, size);
  #endif

  //free(prev);
  //free(next);  
}

void slave(int rank) {
  /*************    Processos escravos     *************/

  // Recebendo número de linhas, passos e tamanho
  int lines, size, steps, num_proc, count;
  MPI_Status st;
  int info[3];

  MPI_Comm_size(MPI_COMM_WORLD, &num_proc);

  MPI_Bcast(info, 3, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Barrier(MPI_COMM_WORLD);
  lines = info[0];
  size = info[1];
  steps = info[2];
  int teste = 0;
  
  if (rank == num_proc-1) {
    /****************    Último processo     ****************/
    // Trabalha com size-1<=
    MPI_Recv(&lines, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, NULL);

    cell_t * prev = (cell_t *) malloc(sizeof(cell_t)*size*lines+1);
    cell_t * next = (cell_t *) malloc(sizeof(cell_t)*size*lines+1);

    for (int i = 0; i < steps; ++i) {
      MPI_Recv(prev, (lines+1)*size, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD, &st);

      //printf("----------------------- step %d\n", i);
      //print(prev, size, lines+1); 
      //printf("----------------------- step %d\n", i);

      // lines +1 porque end é nao inclusivo
      play(prev, next, size, lines+1, 1, lines+1);

      //printf("----------------------- play %d\n", i);
      //print(next+size, size, lines); 
      //printf("----------------------- play %d\n", i);

      MPI_Send(next+size, lines*size, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD);
      MPI_Barrier(MPI_COMM_WORLD);
    }

    //free(next);
    //free(prev);

  } else if (rank == 1) {
    /****************    Primeiro processo     ****************/
    // Trabalha com as linhas 0 <= i < lines
    cell_t * prev = (cell_t *) malloc(sizeof(cell_t)*size*lines+1);
    cell_t * next = (cell_t *) malloc(sizeof(cell_t)*size*lines+1);

    for (int i = 0; i < steps; ++i) {
      MPI_Recv(prev, (lines+1)*size, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD, &st);

      //printf("----------------------- step %d\n", i);
      //print(prev, size, lines+1); 
      //printf("----------------------- step %d\n", i);

      //int size, int lines, int start, int end
      play(prev, next, size, lines+1, 0, lines);

      //printf("----------------------- PLAY %d\n", i);
      //print(next, size, lines); 
      //printf("----------------------- play %d\n", i);


      MPI_Send(next, lines*size, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD);
      MPI_Barrier(MPI_COMM_WORLD);
    }

    //free(next);
    //free(prev);

  } else {
    /****************     Processos do meio      ****************/
    cell_t * prev = (cell_t *) malloc(sizeof(cell_t)*size*lines+2);
    cell_t * next = (cell_t *) malloc(sizeof(cell_t)*size*lines+2);

    for (int i = 0; i < steps; ++i) {
      MPI_Recv(prev, (lines+2)*size, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD, &st);
      
      //printf("----------------------- step %d\n", i);
      //print(prev, size, lines+2); 
      //printf("----------------------- step %d\n", i);

      //int size, int lines, int start, int end
      play(prev, next, size, lines+2, 1, lines+1);

      //printf("----------------------- play %d\n", i);
      //print(next+size, size, lines); 
      //printf("----------------------- play %d\n", i);

      MPI_Send(next+size, lines*size, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD);
      MPI_Barrier(MPI_COMM_WORLD);
    }

    //free(next);
    //free(prev);
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

  MPI_Barrier(MPI_COMM_WORLD);
  printf("========> RANK %d TERMINOU \n", rank);
  MPI_Finalize();
}

/* return the number of on cells adjacent to the i,j cell */
int adjacent_to (cell_t * board, int size, int lines, int i, int j) {
  int count=0;

  if (i > 0) {
    count += board[size*(i-1) + j];
    if (j > 1) count += board[size*(i-1) + j-1];
    if (j+1 < size) count += board[size*(i-1) + j+1];
  }

  if (i+1 < lines) {
    count += board[size*(i+1) + j];
    if (j+1 < size) count += board[size*(i+1) + j+1];
  }

  if (j > 1) {
    count += board[size*i + j-1];
    if (i+1 < lines) count += board[size*(i+1) + j-1];
  }

  if (j+1 < size) {
    count += board[size*i + j+1];
  }

  return count;
}

void play (cell_t * board, cell_t * newboard, int size, int lines, int start, int end) {
  int i, j, a, linesIndex, index;
  /* for each cell, apply the rules of Life */
  /*  Por utilizar um array unidimensional para a matriz, é preciso converter [i][j]
   *  para um único index [], usando [size*i+j]
   */
  for (i=start; i<end; i++) {
    linesIndex = size*i;
    for (j=0; j<size; j++) {
      index = linesIndex + j;
      a = adjacent_to (board, size, lines, i, j);
      newboard[index] = (a==2) ? board[index] : (a==3);
    }
  }
}

/* print the life board */
void print (cell_t * board, int size, int lines) {
  int i, j;
  /* for each row */
  for (j=0; j<lines; j++) {
    /* print each column position... */
    for (i=0; i<size; i++)
    printf ("%c", board[size*j+i] ? 'x' : ' ');
    /* followed by a carriage return */
    printf ("\n");
  }
}

/* read a file into the life board */
void read_file (FILE * f, cell_t * board, int size) {
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
    board[size*j+i] = s[i] == 'x';

  }
}

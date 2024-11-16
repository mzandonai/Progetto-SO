/*  TRISERVER */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <signal.h>
#include <sys/signal.h>
#include <ctype.h>
#include <string.h>
#include <stdbool.h>

#define SHM_KEY 4321
#define SEM_KEY 8765
#define SIZE 1024

// Prototipi
void startup_controls(int argc, char *argv[]);
void handle_ctrl(int sig);
void cleanup();
bool victory();
bool draw();

int matrix_dim = 0;

int timeout = 0;
char player1;
char player2;
bool bot = false;

int ctrl_count = 0;

int semid;
int shmid;
int *shared_memory;
struct sembuf sop = {0, 0, 0};

// Controllo iniziale
void startup_controls(int argc, char *argv[])
{
    if (argc < 4)
    {
        printf("Utilizzo: ./TriServer <timeout> <simbolo 1> <simbolo 2>\n");
        exit(EXIT_FAILURE);
    }
    else
    {
        timeout = atoi(argv[1]);
        if (timeout < 0)
        {
            printf("Timer non può essere negativo.\n");
            exit(EXIT_FAILURE);
        }
        else if (timeout == 0)
        {
            printf("Timer nullo inserito\n");
        }
        else if (timeout > 0)
        {
            printf("Timer inserito: %d\n", timeout);
        }

        if (strlen(argv[2]) != 1 || strlen(argv[3]) != 1)
        {
            printf("Il simbolo del giocatore deve essere singolo.\n");
            exit(EXIT_FAILURE);
        }
        else
        {
            player1 = argv[2][0];
            player2 = argv[3][0];
        }
    }

    printf("Player 1: %c\n", player1);
    printf("Player 2: %c\n", player2);
}

// Gestione del CTRL + C
void sig_handle_ctrl(int sig)
{
    if (ctrl_count == 0)
    {
        printf("\nHai premuto CTRL+C, premi di nuovo per terminare\n");
        ctrl_count++;
    }
    else
    {
        printf("\nProgramma terminato\n");
        // Rimozione
        cleanup();
        exit(0);
    }
}

// Cancellazione del segmento di memoria
void cleanup()
{
    // Deallocazione memoria condivisa
    if (shmdt(shared_memory) == -1)
    {
        perror("Errore durante la deallocazione della memoria condivisa (shmdt)");
    }
    else
    {
        printf("Memoria condivisa deallocata con successo.\n");
    }

    // Rimuove area di memoria condivisa
    if (shmctl(shmid, IPC_RMID, NULL) == -1)
    {
        perror("Errore durante la rimozione dell'area di memoria condivisa (shmctl)");
    }
    else
    {
        printf("Area di memoria condivisa rimossa con successo.\n");
    }

    // Deallocazione semaforo
    if (semctl(semid, 0, IPC_RMID) == -1)
    {
        perror("Errore durante la deallocazione del semaforo (semctl)");
    }
    else
    {
        printf("Semaforo deallocato con successo.\n");
    }
}

bool victory()
{
    // controllo orizzontale

    // controllo verticale

    // controllo diagonale
}

bool draw()
{
    // controllo le celle occupate della matrice

    // se tutte le celle sono occupate e nessuno ha visto c'è pareggio
}

// Main program
int main(int argc, char *argv[])
{
    signal(SIGINT, sig_handle_ctrl);

    // Controllo iniziale dei parametri
    startup_controls(argc, argv);

    // Richiesta dimensione matrice
    printf("\nInserisci la dimensione della matrice: ");
    scanf("%i", &matrix_dim);

    // Creazione memoria condivisa
    shmid = shmget(SHM_KEY, SIZE, IPC_CREAT | 0660);
    if (shmid == -1)
    {
        perror("shmget");
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("Memoria creata\n");
    }

    shared_memory = (int *)shmat(shmid, NULL, 0);
    if (shared_memory == (int *)-1)
    {
        perror("shmat");
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("Spazio di memoria creato\n");
    }

    /* inizializzazione shared memory */
    shared_memory[0] = player1;  // player1
    shared_memory[1] = player2;  // player2
    shared_memory[2] = getpid(); // pid del server
    shared_memory[3] = 0;        // turno corrente
    shared_memory[4] = 0;        // stato del gioco
    shared_memory[5] = matrix_dim;

    int board_sz = matrix_dim * matrix_dim;
    for (int i = 0; i < board_sz; i++)
    {
        shared_memory[6 + i] = ' ';
    }

    // Creazione semaforo
    semid = semget(SEM_KEY, 1, IPC_CREAT | 0666);
    if (semid < 0)
    {
        perror("semget");
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("Semaforo creato\n");
    }

    /*
        imposto valore semaforo a -2, incremento quando un
        giocatore si connette alla partia
    */

    if (semctl(semid, 0, SETVAL, 0) == -1)
    {
        perror("Errore nell'assegnazione -2 al semaforo\n");
        cleanup();
        exit(EXIT_FAILURE);
    }

    // Il gioco va avanti fin quando un giocatore vince o pareggia
    printf("In attesa di due giocatori per iniziare la partita...\n");

    struct sembuf sop;
    sop.sem_num = 0;
    sop.sem_op = -2;
    sop.sem_flg = 0;

    if (semop(semid, &sop, 1) == -1)
    {
        perror("Errore in semop durante l'attesa\n");
    }

    printf("Due giocatori connessi...la parita ha inizio\n");

    while (1)
    {
        /* code */
    }

    // Rimozione memoria, semaforo
    cleanup();
    return 0;
}

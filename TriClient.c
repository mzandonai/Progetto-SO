/* TRICLIENT */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <time.h>

#define SHM_KEY 4321
#define SEM_KEY 8765
#define SIZE 1024
#define PID1
#define PID2

int semid;
int semval;
int shmid;
int *shared_memory;
struct sembuf sb;
char symbol;

bool timer_expired = false;
int ctrl_count = 0; // CTRL + C contatore

bool bot = false; // giocatore automatico
bool asterisk = false;

// Cancellazione del segmento di memoria
void cleanup()
{
    // Deallocazione memoria condivisa
    if (shared_memory != NULL)
    {
        if (shmdt(shared_memory) == -1)
        {
            perror("Errore durante distacco mem. condivisa");
            exit(EXIT_FAILURE);
        }
        else
        {
            printf("Mem. condivisa distaccata\n");
        }
    }
}

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

void sig_handle_timer(int sig)
{
    // gestore del timer di inserimento mossa
    printf("\nTempo scaduto\n");
    timer_expired = true;
}

void sig_server_closed(int sig)
{
    if (sig == SIGUSR1)
    {
        printf("\nIl server è stato disconnesso.\n");
        cleanup();
        exit(EXIT_FAILURE);
    }

    if (sig == SIGUSR2)
    {
        printf("Il tuo avversario ha abbandonato.\n");
        exit(EXIT_FAILURE);
    }
}

void startup_controls(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Utilizzo: ./TriClient <nome utente> [*]\n");
        exit(EXIT_FAILURE);
    }
    else if (argc > 3)
    {
        fprintf(stderr, "Errore: troppi argomenti forniti.\n");
        fprintf(stderr, "Utilizzo: ./TriClient <nome utente> [*]\n");
        exit(EXIT_FAILURE);
    }

    if (argc == 3 && strcmp(argv[2], "*") == 0)
    {
        printf("Modalità bot attiva.\n");
        bot = true; // Imposta il bot attivo
    }
    else if (argc == 3)
    {
        fprintf(stderr, "Errore: terzo argomento non valido.\n");
        fprintf(stderr, "Utilizzo: ./TriClient <nome utente> [*]\n");
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("Modalità giocatore reale attiva.\n");
        bot = false; // Giocatore reale
    }
}

void correct_move()
{
    if (bot)
    {
        // Genero una mossa casuale
    }
    else
    {
        // Chiedo all'utente l'inserimento di una mossa
    }
}

void print_matrix()
{
    printf("Tabellone corrente: \n");
    int dim = shared_memory[5];
    for (int i = 0; i < dim; i++)
    {
        for (int j = 0; j < dim; j++)
        {
            char cell = shared_memory[6 + i * dim + j];
            printf(" %c ", cell == ' ' ? '.' : cell); // Mostra '.' per celle vuote
            if (j < dim - 1)
                printf("|");
        }
        printf("\n");
        if (i < dim - 1)
        {
            for (int k = 0; k < dim; k++)
            {
                printf("---");
                if (k < dim - 1)
                    printf("+");
            }
            printf("\n");
        }
    }
}

int main(int argc, char *argv[])
{
    startup_controls(argc, argv);    // Controlli di startup
    signal(SIGINT, sig_handle_ctrl); // Gestore del CTRL + C
    signal(SIGUSR1, sig_server_closed);

    // Memoria condivisa
    shmid = shmget(SHM_KEY, SIZE, 0600);
    if (shmid == -1)
    {
        perror("Errore durante la connessione alla memoria\n");
        exit(EXIT_FAILURE);
    }
    shared_memory = (int *)shmat(shmid, NULL, 0);
    if (shared_memory == (int *)-1)
    {
        perror("Errore nel collegamento alla memoria\n");
        exit(EXIT_FAILURE);
    }

    // Semafori
    semid = semget(SEM_KEY, 1, 0600);
    if (semid == -1)
    {
        perror("Errore nella connessione al semaforo\n");
        exit(EXIT_FAILURE);
    }

    /*
        Gioco in coppia
        // ogni client che viene eseguito fa un'operazione di incremento sul semaforo semid,
        // quando vengono eseguiti 2 client allora il semaforo va a 0 e il server continua
    */
    if (asterisk == false && bot == false)
    {
        sb.sem_op = 1;
        semop(semid, &sb, 1);
        semval = semctl(semid, 0, GETVAL);
        if (semval == 1)
        {
            shared_memory[PID1] = getpid();
            printf("In attesa di un altro giocatore...\n");
            symbol = shared_memory[0];
        }
        else
        {
            shared_memory[PID2] = getpid();
            printf("\n");
            printf("\n - | Secondo giocatore | - \n");
            symbol = shared_memory[1];
        }
    }
    else
    {
        if (!bot)
        {
            printf("Sta giocando in in SINGLE PLAYER\n");
            printf("Questo è il tuo simbolo: %c\n", symbol);
        }
    }

    // Il while controlla e dice chi ha visto o se c'è pareggio
    while (1)
    {
        /* code */
    }

    cleanup();
    return 0;
}
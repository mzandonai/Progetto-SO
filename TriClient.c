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

#define PID1 3
#define PID2 4

int semid;
int semval;
int shmid;
int *shared_memory;
char symbol;
int player;
int board_start = 9;
struct sembuf sb;

bool timer_expired = false;
int ctrl_count = 0; // CTRL + C contatore

bool bot = false; // giocatore automatico

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
    }
}

// Gestore del CTRL + L
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
        // Segnale kill
        cleanup();
        exit(0);
    }
}

void sig_server_closed(int sig) // SIGTERM
{
    printf("\n");
    printf("\n - ALERT : Server disconnesso o chiuso forzatamente -\n");
    cleanup();
    exit(0);
}

void startup_controls(int argc, char *argv[])
{
    // Controlla il numero di argomenti forniti
    if (argc < 2 || argc > 3)
    {
        fprintf(stderr, "Errore: numero di argomenti non valido.\n");
        fprintf(stderr, "  Modalità giocatore doppio: ./TriClient <username>\n");
        fprintf(stderr, "  Modalità bot: ./TriClient <username> *\n");
        fprintf(stderr, "Nota: Usa l'asterisco escapato (es. \\* o \"*\") per evitare problemi di interpretazione nella shell.\n");
        exit(0);
    }

    // Controlla il formato corretto degli argomenti
    if (argc == 2)
    {
        printf("Modalità giocatore doppio attiva.\n");
        bot = false; // Modalità giocatore reale
    }
    else if (argc == 3 && strcmp(argv[2], "*") == 0)
    {
        printf("Modalità bot attiva.\n");
        bot = true; // Modalità bot
    }
    else
    {
        fprintf(stderr, "Errore: argomenti non validi.\n");
        fprintf(stderr, "  Modalità giocatore doppio: ./TriClient <username>\n");
        fprintf(stderr, "  Modalità bot: ./TriClient <username> *\n");
        fprintf(stderr, "Nota: Usa l'asterisco escapato (es. \\* o \"*\") per evitare problemi di interpretazione nella shell.\n");
        exit(0);
    }
}

void correct_move()
{
    int dim = shared_memory[8]; // Dimensione della matrice
    int row, col;               // Righe e colonne
    bool valid_move = false;    // Mossa valida messa a false

    // Timer
    timer_expired = false; // Timer reset
    alarm(shared_memory[7]);

    if (bot)
    {
        printf("Il bot sta effettuando la sua mossa...\n");
        srand(time(NULL));
        while (!valid_move)
        {
            // Genera una mossa casuale
            row = rand() % dim;
            col = rand() % dim;
            int index = board_start + row * dim + col; // Calcola l'indice nella memoria condivisa

            if (shared_memory[index] == ' ')
            {
                shared_memory[index] = shared_memory[5] == 0 ? shared_memory[0] : shared_memory[1];
                printf("Il bot ha giocato in posizione (%d, %d)\n", row, col);
                valid_move = true;
                alarm(0);
            }
        }
    }
    else
    {
        while (!valid_move)
        {
            printf("Inserisci la riga e la colonna per la tua mossa (es. 1 2): ");
            scanf("%d %d", &row, &col);

            if (row >= 0 && row < dim && col >= 0 && col < dim) // Controlla i limiti
            {
                int index = board_start + row * dim + col; // Calcola l'indice nella memoria condivisa
                if (shared_memory[index] == ' ')           // Controlla che la cella sia vuota
                {
                    // Inserisce il simbolo corretto per il giocatore
                    shared_memory[index] = shared_memory[5] == 0 ? shared_memory[0] : shared_memory[1];
                    valid_move = true;
                    alarm(0);
                }
                else
                {
                    printf("Cella già occupata. Riprova.\n");
                }
            }
            else
            {
                printf("Mossa non valida. Riprova.\n");
            }
        }
    }

    // Passa il turno all'altro giocatore
    shared_memory[5] = (shared_memory[5] == 0) ? 1 : 0;
}

void print_matrix()
{
    printf("Tabellone corrente: \n");
    int dim = shared_memory[8]; // dimensione della matrice
    for (int i = 0; i < dim; i++)
    {
        for (int j = 0; j < dim; j++)
        {
            char cell = shared_memory[board_start + i * dim + j];
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
    signal(SIGTERM, sig_server_closed);

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
    if (bot == false)
    {
        sb.sem_op = 1;
        semop(semid, &sb, 1);
        semval = semctl(semid, 0, GETVAL);
        if (semval == 1)
        {
            shared_memory[PID1] = getpid();
            printf("In attesa di un altro giocatore...\n");
            symbol = shared_memory[0];
            player = 0;
            sb.sem_op--;
            while (semval != 0)
            {
                semval = semctl(semid, 0, GETVAL);
            }
        }
        else
        {
            shared_memory[PID2] = getpid();
            symbol = shared_memory[1];
            player = 1;
        }
        printf("Il tuo simbolo è %c\n", symbol);
    }
    else
    {
        if (!bot)
        {
            symbol = shared_memory[0];
            player = 0;
            printf("Sta giocando contro il bot\n");
            printf("Questo è il tuo simbolo: %c\n", symbol);
        }
        else
        {
            symbol = shared_memory[1];
            player = 1;
        }
        sb.sem_op = 2;
        semop(semid, &sb, 1);
    }

    int last_turn = -1;
    while (1)
    {
        if (shared_memory[5] != last_turn)
        {
            print_matrix();
            last_turn = shared_memory[5];
        }

        if (!bot && shared_memory[5] == player)
        {
            printf("Il tuo turno (%c)\n", symbol);
            correct_move();
        }

        sleep(1);
    }

    cleanup();
    return 0;
}
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

int timeout = 0;
int ctrl_count = 0; // CTRL + C contatore

bool bot = false; // giocatore automatico
bool computer = false;

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

// Controlli iniziali
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

// Gestore del CTRL + L --> SIGURS1
void sig_handle_ctrl(int sig)
{
    if (ctrl_count == 0)
    {
        printf("\nHai premuto CTRL+C, premi di nuovo per terminare\n");
        ctrl_count++;
    }
    else
    {
        printf("\n");
        printf("---------------------------------------------------\n");
        printf("    G A M E   O V E R : Hai perso! Ti sei ritirato\n");
        printf("---------------------------------------------------\n");
        printf("\n");
        // Segnale kill
        shared_memory[6] = 3; // client abbandonato
        kill(shared_memory[2], SIGUSR1);
        cleanup();
        exit(0);
    }
}

// Ricevuto SIGUSR1 chiude il client
void sig_client_closed(int sig) // SIGUSR1
{
    printf("\n");
    printf("---------------------------------------------------------------------\n");
    printf("    G A M E   O V E R : Hai vinto! Il tuo avvversario ha abbandonato\n");
    printf("---------------------------------------------------------------------\n");
    printf("\n");
    cleanup();
    exit(0);
}

// Ricevuto SIGTERM chiude il client
void sig_server_closed(int sig) // SIGTERM
{
    if (shared_memory[6] == 1)
    {
        printf("\n");
        printf("-------------------------------------------------------\n");
        printf("    G A M E   O V E R : Vittoria!\n");
        printf("-------------------------------------------------------\n");
        printf("\n");
        exit(0);
    }
    else if (shared_memory[6] == 2)
    {
        printf("\n");
        printf("---------------------------------\n");
        printf("    G A M E   O V E R : Pareggio!\n");
        printf("---------------------------------\n");
        printf("\n");
    }
    else
    {
        printf("\n");
        printf("-------------------------------------------------------\n");
        printf("    G A M E   O V E R : Partita terminata forzatamente\n");
        printf("-------------------------------------------------------\n");
        printf("\n");
    }
    cleanup();
    exit(0);
}

// Quando il timer scade avvisa il server con SIGUSR2
void sig_handle_timeout(int sig) // ALARM CLOCK --> SIGUSR2
{
    printf("\n");
    printf("-------------------------------------------------\n");
    printf("    G A M E   O V E R : Hai perso! Timer scaduto\n");
    printf("-------------------------------------------------\n");
    printf("\n");
    kill(shared_memory[2], SIGUSR2);
    cleanup();
    exit(0);
}

void sig_receive_timeout(int sig)
{
    printf("\n");
    printf("-----------------------------------------------\n");
    printf("    G A M E   O V E R : Hai vinto per timeout!\n");
    printf("-----------------------------------------------\n");
    printf("\n");
    cleanup();
    exit(0);
}

void correct_move()
{
    int dim = shared_memory[8]; // Dimensione della matrice
    int row, col;               // Righe e colonne
    bool valid_move = false;    // Mossa valida messa a false

    // Timer
    alarm(shared_memory[7]);

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
            else if (!bot)
            {
                printf("Cella già occupata. Riprova.\n");
            }
        }
        else if (!bot)
        {
            printf("Mossa non valida. Riprova.\n");
        }
    }

    // Passa il turno all'altro giocatore
    shared_memory[5] = (shared_memory[5] == 0) ? 1 : 0;
}

void print_matrix()
{
    printf("\n");
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
    startup_controls(argc, argv); // Controlli di startup

    signal(SIGINT, sig_handle_ctrl); // Gestore del CTRL + C

    signal(SIGTERM, sig_server_closed); // Gestore chiusura Server
    signal(SIGUSR1, sig_client_closed); // Gestore chiusura Client

    signal(SIGALRM, sig_handle_timeout);  // Gestione timer
    signal(SIGUSR2, sig_receive_timeout); // Gestore ricezione da server

    // Memoria condivisa
    shmid = shmget(SHM_KEY, SIZE, 0600);
    if (shmid == -1)
    {
        perror("Errore durante la connessione alla memoria\n");
        exit(0);
    }

    shared_memory = (int *)shmat(shmid, NULL, 0);
    if (shared_memory == (int *)-1)
    {
        perror("Errore nel collegamento alla memoria\n");
        exit(0);
    }

    // Semafori
    semid = semget(SEM_KEY, 1, 0600);
    if (semid == -1)
    {
        perror("Errore nella connessione al semaforo\n");
        exit(0);
    }

    // Ottengo il timeout impostato:
    timeout = shared_memory[7];

    if (!computer && !bot)
    {
        sb.sem_op = 1;
        semop(semid, &sb, 1);
        semval = semctl(semid, 0, GETVAL);

        if (semval == 1)
        {
            shared_memory[PID1] = getpid();
            printf("\n");
            printf("\nIn attesa di un altro giocatore...\n");
            printf("\n");
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
            printf("\n");
            printf("\nSei il secondo giocatore...\n");
            printf("\n");
            symbol = shared_memory[1];
            player = 1;
        }

        printf("\n");
        printf("\nIl tuo simbolo è %c\n", symbol);
        printf("\n");
    }
    else
    {
        kill(shared_memory[2], SIGTERM);
        if (!computer)
        {
            symbol = shared_memory[0];
            player = 0;
            printf("\nIl tuo simbolo è: %c\n", symbol);
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
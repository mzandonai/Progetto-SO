/*************************************
 *Matricola
 * Matteo Zandonai
 * 09/12/2024
 *************************************/

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
#define TURN 5
#define STATUS 6

int whoami = 0;
int timeout = 0;
int ctrl_count = 0; // CTRL + C contatore

int semid;
int semval;
int shmid;
int *shared_memory;
char symbol;
int player;
int board_start = 9;

struct sembuf sb;

bool asterisco = false; // giocatore automatico
bool sono_CPU = false;

// Cancellazione del segmento di memoria
void cleanup()
{
    // Deallocazione memoria condivisa
    if (!sono_CPU)
    {
        if (shmdt(shared_memory) == -1)
        {
            perror("Errore durante distacco mem. condivisa");
        }
    }
    else
    {
        if (shmdt(shared_memory) == -1)
        {
            perror("Errore durante distacco mem. condivisa");
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
        exit(0);
    }

    // Controlla il formato corretto degli argomenti
    if (argc == 2)
    {
        asterisco = false;
    }
    else if (argc == 3 && strcmp(argv[2], "*") == 0)
    {
        asterisco = true; // Modalità bot
    }
    else
    {
        fprintf(stderr, "Errore: argomenti non validi.\n");
        fprintf(stderr, "  Modalità giocatore doppio: ./TriClient <username>\n");
        fprintf(stderr, "  Modalità bot: ./TriClient <username> *\n");
        exit(0);
    }
}

// Gestore del CTRL + L --> SIGINT
void sig_handle_ctrl(int sig)
{
    if (!sono_CPU)
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
    else
    {
        cleanup();
        _exit(0);
    }
}

// Ricevuto SIGUSR1 chiude il client
void sig_client_closed(int sig) // SIGUSR1
{
    if (!sono_CPU)
    {
        printf("\n");
        printf("---------------------------------------------------------------------\n");
        printf("    G A M E   O V E R : Hai vinto! Il tuo avversario ha abbandonato\n");
        printf("---------------------------------------------------------------------\n");
        printf("\n");
        cleanup();
        exit(0);
    }
    else
    {
        cleanup();
        _exit(0);
    }
}

// Ricevuto SIGTERM chiude il client
void sig_server_closed(int sig) // SIGTERM
{
    if (asterisco && !sono_CPU)
    {
        if (shared_memory[STATUS] == 1)
        {
            printf("\nTurno attuale: %d\n", shared_memory[TURN]);

            if (shared_memory[TURN] == 0)
            {
                printf("\n");
                printf("-------------------------------------------------------\n");
                printf("    G A M E   O V E R : Hai perso [BOT]!\n");
                printf("-------------------------------------------------------\n");
                printf("\n");
                cleanup();
                exit(0);
            }
            else
            {
                printf("\n");
                printf("-------------------------------------------------------\n");
                printf("    G A M E   O V E R : Vittoria [BOT]!\n");
                printf("-------------------------------------------------------\n");
                printf("\n");
                cleanup();
                exit(0);
            }
        }
        else if (shared_memory[STATUS] == 2)
        {
            printf("\n");
            printf("---------------------------------\n");
            printf("    G A M E   O V E R : Pareggio!\n");
            printf("---------------------------------\n");
            printf("\n");
            cleanup();
            exit(0);
        }
        else
        {
            printf("\n");
            printf("-------------------------------------------------------\n");
            printf("    G A M E   O V E R : Partita terminata forzatamente\n");
            printf("-------------------------------------------------------\n");
            printf("\n");
            cleanup();
            exit(0);
        }
    }
    else if (!asterisco && !sono_CPU)
    {
        if (shared_memory[STATUS] == 1)
        {
            printf("\nTurno attuale: %d\n", shared_memory[TURN]);

            if (shared_memory[TURN] == player)
            {
                printf("\n");
                printf("-------------------------------------------------------\n");
                printf("    G A M E   O V E R : Hai perso!\n");
                printf("-------------------------------------------------------\n");
                printf("\n");
                cleanup();
                exit(0);
            }
            else
            {
                printf("\n");
                printf("-------------------------------------------------------\n");
                printf("    G A M E   O V E R : Vittoria!\n");
                printf("-------------------------------------------------------\n");
                printf("\n");
                cleanup();
                exit(0);
            }
        }
        else if (shared_memory[STATUS] == 2)
        {
            printf("\n");
            printf("---------------------------------\n");
            printf("    G A M E   O V E R : Pareggio!\n");
            printf("---------------------------------\n");
            printf("\n");
            cleanup();
            exit(0);
        }
        else
        {
            printf("\n");
            printf("-------------------------------------------------------\n");
            printf("    G A M E   O V E R : Partita terminata forzatamente\n");
            printf("-------------------------------------------------------\n");
            printf("\n");
            cleanup();
            exit(0);
        }
    }
    else if (sono_CPU)
    {
        cleanup();
        _exit(0);
    }
}

// Quando il timer scade avvisa il server
void sig_handle_timeout(int sig) // ALARM CLOCK
{
    if (!sono_CPU)
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
    else
    {
        cleanup();
        _exit(0);
    }
}

void sig_receive_timeout(int sig) // SIGUSR2
{
    if (!sono_CPU)
    {
        printf("\n");
        printf("-----------------------------------------------\n");
        printf("    G A M E   O V E R : Hai vinto per timeout!\n");
        printf("-----------------------------------------------\n");
        printf("\n");
        cleanup();
        exit(0);
    }
    else
    {
        cleanup();
        _exit(0);
    }
}

void print_matrix()
{
    printf("\n");
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

void correct_move()
{
    char input[4];
    int dim = shared_memory[8]; // Dimensione della matrice
    int row, col;               // Righe e colonne
    bool valid_move = false;    // Mossa valida messa a false

    while (!valid_move)
    {
        if (sono_CPU)
        {
            row = rand() % dim;
            col = rand() % dim;
            int index = board_start + row * dim + col;
            if (shared_memory[index] == ' ') // Controlla se la cella è vuota
            {
                // Inserisce il simbolo corretto per il bot
                shared_memory[index] = shared_memory[TURN] == 0 ? shared_memory[0] : shared_memory[1];
                valid_move = true;
            }
        }
        else
        {
            // Timer
            alarm(shared_memory[7]);
            printf("\n");
            printf("Il tuo turno (%c)\n", symbol);
            printf("Inserisci la riga e la colonna per la tua mossa (es. 1 2): ");

            memset(input, 0, sizeof(input));

            if (fgets(input, sizeof(input), stdin) != NULL)
            {
                if (strchr(input, '\n') == NULL)
                {
                    int ch;
                    while ((ch = getchar()) != '\n' && ch != EOF)
                        ;
                }

                char *error_message = NULL;
                if (sscanf(input, "%d %d", &row, &col) == 2)
                {
                    if (row >= 0 && row < dim && col >= 0 && col < dim) // Controlla i limiti
                    {
                        int index = board_start + row * dim + col; // Calcola l'indice nella memoria condivisa
                        if (shared_memory[index] == ' ')           // Controlla che la cella sia vuota
                        {
                            // Inserisce il simbolo corretto per il giocatore
                            shared_memory[index] = shared_memory[TURN] == 0 ? shared_memory[0] : shared_memory[1];
                            valid_move = true;
                            alarm(0);

                            if (!sono_CPU)
                            {
                                printf("\n");
                                printf("\nTabellone dopo la tua mossa:\n");
                                print_matrix();
                            }
                        }
                        else if (!sono_CPU)
                        {
                            error_message = "Cella già occupata. Riprova.";
                        }
                    }
                    else if (!sono_CPU)
                    {
                        error_message = "Mossa fuori range. Riprova.";
                    }
                }
                else if (!sono_CPU)
                {
                    error_message = "Input non valido. Riprova.";
                }

                if (error_message != NULL)
                {
                    printf("\n%s\n", error_message);
                }
            }
            else if (!sono_CPU)
            {
                printf("\nErrore nell'input. Riprova\n");
            }
        }
    }

    // Passa il turno all'altro giocatore
    shared_memory[TURN] = (shared_memory[TURN] == 0) ? 1 : 0;
}

void how_to_play()
{
    int dim = shared_memory[8]; // Dimensione della matrice
    if (asterisco)
    {
        // Spiegazione del gioco
        printf("Benvenuto nel gioco del Tris!\n");
        printf("Ecco come giocare in modalità BOT:\n");
        printf("1. Il tuo obiettivo sarà quello di sconfiggere il bot.\n");
        printf("2. Per fare una mossa, scegli la combinazione di riga e colonna della cella.\n");
        printf("3. Il primo giocatore che allinea %d simboli in riga, colonna o diagonale vince!\n", dim);
        printf("4. Hai %d secondi per effettuare la mossa\n\n", timeout);
        // Stampa della matrice di gioco
        printf("Matrice di gioco:\n");
        for (int i = 0; i < dim; i++)
        {
            for (int j = 0; j < dim; j++)
            {
                // Stampa la combinazione riga e colonna
                printf(" %d%d ", i, j);
                if (j < dim - 1)
                    printf("|");
            }
            printf("\n");

            // Stampa la separazione tra le righe
            if (i < dim - 1)
            {
                for (int k = 0; k < dim; k++)
                {
                    printf("----");
                    if (k < dim - 1)
                        printf("+");
                }
                printf("\n");
            }
        }
    }
    else
    {
        // Spiegazione del gioco
        printf("Benvenuto nel gioco del Tris!\n");
        printf("Ecco come giocare:\n");
        printf("1. Ogni giocatore alterna il turno per piazzare il proprio simbolo.\n");
        printf("2. Per fare una mossa, scegli la combinazione di riga e colonna della cella.\n");
        printf("3. Il primo giocatore che allinea %d simboli in riga, colonna o diagonale vince!\n", dim);
        printf("4. Hai %d secondi per effettuare la mossa\n\n", timeout);

        // Stampa della matrice di gioco
        printf("Matrice di gioco:\n");
        for (int i = 0; i < dim; i++)
        {
            for (int j = 0; j < dim; j++)
            {
                // Stampa la combinazione riga e colonna
                printf(" %d%d ", i, j);
                if (j < dim - 1)
                    printf("|");
            }
            printf("\n");

            // Stampa la separazione tra le righe
            if (i < dim - 1)
            {
                for (int k = 0; k < dim; k++)
                {
                    printf("----");
                    if (k < dim - 1)
                        printf("+");
                }
                printf("\n");
            }
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
        printf("Errore durante la connessione alla memoria\n");
        exit(0);
    }

    shared_memory = (int *)shmat(shmid, NULL, SHM_RND);
    if (shared_memory == (int *)-1)
    {

        printf("Errore nel collegamento alla memoria\n");
        exit(0);
    }

    // Semafori
    semid = semget(SEM_KEY, 1, 0600);
    if (semid == -1)
    {
        printf("ERRORE: Errore nella connessione al semaforo\n");
        exit(0);
    }

    // Ottengo il timeout impostato:
    timeout = shared_memory[7];
    if (asterisco)
    {
        kill(shared_memory[2], SIGTERM);
    }

    whoami = getpid();
    if (whoami == shared_memory[PID2])
    {
        sono_CPU = true;
    }
    else
    {
        sono_CPU = false;
    }

    if (!asterisco && !sono_CPU)
    {
        sb.sem_num = 0;
        sb.sem_op = 1;
        sb.sem_flg = 0;

        semop(semid, &sb, 1);
        semval = semctl(semid, 0, GETVAL);
        printf("Valore del semaforo: %d\n", semval);

        if (semval == 1)
        {
            shared_memory[PID1] = getpid();
            how_to_play();

            printf("\n");
            printf("\nIn attesa di un altro giocatore...\n");
            printf("\n");
            symbol = shared_memory[0];
            player = 0;
            // sb.sem_op--;
            while (semval != 2)
            {
                semval = semctl(semid, 0, GETVAL);
            }
        }
        else
        {
            shared_memory[PID2] = getpid();
            how_to_play();

            printf("\n");
            printf("\nSei il secondo giocatore, la partita ha inizio...\n");
            printf("\n");
            symbol = shared_memory[1];
            player = 1;
        }

        printf("Il tuo simbolo è %c\n", symbol);
        printf("\n");
    }
    else
    {
        if (!sono_CPU)
        {
            shared_memory[PID1] = getpid();
            how_to_play();
            symbol = shared_memory[0];
            player = 0;

            printf("\nIl tuo simbolo è: %c\n", symbol);
        }
        else
        {
            // bot giocatore automatico
            symbol = shared_memory[1];
            player = 1;
        }

        sb.sem_op = 2;
        semop(semid, &sb, 1);
    }

    while (shared_memory[STATUS] == 0)
    {
        // da sistemare
        if (shared_memory[TURN] == player)
        {
            if (!sono_CPU)
            {
                print_matrix();
            }

            correct_move();
            if (!sono_CPU)
            {
                printf("\nL'avversario sta eseguendo la mossa...\n");
            }
        }

        /*
        if (!sono_CPU && last_turn != 1)
        {
            print_matrix();
            last_turn = 1;
        }

        if (shared_memory[TURN] == player)
        {
            if (!sono_CPU)
            {
                print_matrix();
            }
            correct_move();
            last_turn = 0;
        }
        */

        sleep(1);
    }

    cleanup();
    return 0;
}

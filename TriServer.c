/*************************************
 * VR473622
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
#include <signal.h>
#include <sys/signal.h>
#include <ctype.h>
#include <string.h>
#include <stdbool.h>

#define SHM_KEY 4321
#define SEM_KEY 8765
#define SIZE 1024

#define PID1 3
#define PID2 4

int matrix_dim = 0;
int board_start = 9; // inizio della matrice in mem

int timeout = 0;
char player1;
char player2;
bool computer = false;

int ctrl_count = 0;

int semid;
int shmid;
int *shared_memory;
struct sembuf sop = {0, 0, 0};

// Cancellazione del segmento di memoria
void cleanup()
{
    // Deallocazione memoria condivisa
    if (shmdt(shared_memory) == -1)
    {
        perror("Errore durante la deallocazione della memoria condivisa (shmdt)");
    }

    // Rimuove area di memoria condivisa
    if (shmctl(shmid, IPC_RMID, NULL) == -1)
    {
        perror("Errore durante la rimozione dell'area di memoria condivisa (shmctl)");
    }

    // Deallocazione semaforo
    if (semctl(semid, 0, IPC_RMID) == -1)
    {
        perror("Errore durante la deallocazione del semaforo (semctl)");
    }
    printf("\n");
}

// Generatore del bot
void sig_fork_generator(int sig)
{
    if (shared_memory[6] != 3)
    {
        pid_t bot_pid = fork();
        if (bot_pid < 0)
        {
            perror("Errore bot");
            cleanup();
            exit(0);
        }
        else if (bot_pid == 0)
        {
            // exec
            computer = true;
            execl("./TriClient", "./TriClient", "bot", (char *)NULL);
        }
    }
}

// Controllo iniziale
void startup_controls(int argc, char *argv[])
{
    /*
        TO DO:
        Controllo inserimento interi invece di caratteri
    */

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

// Gestione del CTRL + C --> SIGTERM
void sig_handle_ctrl(int sig)
{
    if (ctrl_count == 0)
    {
        printf("\nHai premuto CTRL+C, premi di nuovo per terminare\n");
        ctrl_count++;
    }
    else
    {
        printf("----------------------------------------------------\n");
        printf("    G A M E   O V E R : Partita terminata\n");
        printf("----------------------------------------------------\n");
        // Rimozione
        if (kill(shared_memory[PID1], 0) == 0)
        {
            kill(shared_memory[PID1], SIGTERM);
        }

        if (kill(shared_memory[PID2], 0) == 0)
        {
            kill(shared_memory[PID2], SIGTERM);
        }

        cleanup();
        exit(0);
    }
}

// Gestore della chiusura del client
void sig_client_closed(int sig)
{
    printf("----------------------------------------------------\n");
    printf("    G A M E   O V E R : Un giocatore ha abbandonato\n");
    printf("----------------------------------------------------\n");
    if (kill(shared_memory[PID1], 0) == 0)
    {
        kill(shared_memory[PID1], SIGUSR1);
    }

    if (kill(shared_memory[PID2], 0) == 0)
    {
        kill(shared_memory[PID2], SIGUSR1);
    }

    cleanup();
    exit(0);
}

// Gestore del timer
void sig_client_timer(int sig)
{
    printf("-------------------------------------------------------\n");
    printf("    G A M E   O V E R : Un client ha perso per timeout\n");
    printf("-------------------------------------------------------\n");
    if (kill(shared_memory[PID1], 0) == 0)
    {
        kill(shared_memory[PID1], SIGUSR2);
    }

    if (kill(shared_memory[PID2], 0) == 0)
    {
        kill(shared_memory[PID2], SIGUSR2);
    }

    cleanup();
    exit(0);
}

// Controllo per la vittoria
bool victory()
{
    int dim = shared_memory[8];
    for (int i = 0; i < dim; i++)
    {
        // controllo righe
        bool row_victory = true;
        for (int j = 0; j < dim; j++)
        {

            if (shared_memory[board_start + i * dim + j] != shared_memory[board_start + i * dim] ||
                shared_memory[board_start + i * dim] == ' ')
            {
                row_victory = false;
                break;
            }
        }
        if (row_victory)
        {
            return true;
        }

        // controllo colonne
        bool col_victory = true;
        for (int j = 0; j < dim; j++)
        {
            if (shared_memory[board_start + j * dim + i] != shared_memory[board_start + i] ||
                shared_memory[board_start + i] == ' ')
            {
                col_victory = false;
                break;
            }
        }
        if (col_victory)
        {
            return true;
        }
    }

    // Controllo diagonale principale
    bool diag_victory1 = true;
    for (int i = 1; i < dim; i++) // Parto da 1 per confrontare con la prima cella
    {
        if (shared_memory[board_start + i * dim + i] != shared_memory[board_start] ||
            shared_memory[board_start] == ' ')
        {
            diag_victory1 = false;
            break;
        }
    }

    // Controllo diagonale secondaria
    bool diag_victory2 = true;
    for (int i = 1; i < dim; i++) // Parto da 1 per confrontare con la prima cella
    {
        if (shared_memory[board_start + i * dim + (dim - 1 - i)] != shared_memory[board_start + (dim - 1)] ||
            shared_memory[board_start + (dim - 1)] == ' ')
        {
            diag_victory2 = false;
            break;
        }
    }

    return diag_victory1 || diag_victory2;
}

// Controllo per il pareggio
bool draw()
{
    // controllo le celle occupate della matrice
    int dim = shared_memory[8];
    int board_sz = dim * dim;

    if (victory())
        return false;

    for (int i = 0; i < board_sz; i++)
    {
        if (shared_memory[board_start + i] == ' ')
        {
            return false;
        }
    }

    return true;
    // se tutte le celle sono occupate e nessuno ha visto c'è pareggio
}

// Main program
int main(int argc, char *argv[])
{
    signal(SIGINT, sig_handle_ctrl);
    signal(SIGUSR1, sig_client_closed);
    signal(SIGUSR2, sig_client_timer);

    signal(SIGTERM, sig_fork_generator); // Gestore gioco bot

    // Controllo iniziale dei parametri
    startup_controls(argc, argv);

    do
    {
        printf("\nInserisci la dimensione della matrice (min 3): ");
        scanf("%i", &matrix_dim);

        if (matrix_dim < 3)
        {
            printf("ERRORE - La dimensione del tabellone deve essere almeno 3\n");
        }

    } while (matrix_dim < 3);

    // Creazione memoria condivisa
    shmid = shmget(SHM_KEY, SIZE, IPC_CREAT | 0660);
    if (shmid == -1)
    {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    shared_memory = (int *)shmat(shmid, NULL, 0);
    if (shared_memory == (int *)-1)
    {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    /* inizializzazione shared memory */
    shared_memory[0] = player1;    // simbolo player1
    shared_memory[1] = player2;    // simbolo player2
    shared_memory[2] = getpid();   // pid del server
    shared_memory[3] = 0;          // PID client1
    shared_memory[4] = 0;          // PID client2
    shared_memory[5] = 0;          // turno corrente (0 o 1)
    shared_memory[6] = 0;          // stato del gioco (0 start, 1 vittoria, 2 pareggio, 3 client abbandona)
    shared_memory[7] = timeout;    // timeout
    shared_memory[8] = matrix_dim; // dimensione matrice

    // Init della matrice
    int board_sz = matrix_dim * matrix_dim;
    for (int i = 0; i < board_sz; i++)
    {
        shared_memory[board_start + i] = ' ';
    }

    // Creazione semaforo
    semid = semget(SEM_KEY, 1, IPC_CREAT | 0666);
    if (semid < 0)
    {
        perror("semget");
        exit(EXIT_FAILURE);
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

    shared_memory[6] = 0; // Gioco iniziato
    while (1)
    {
        /*
            shared_memory[6] = 0 // gioco iniziato
            shared_memory[6] = 1 // vittoria
            shared_memory[6] = 2 // pareggio
            shared_memory[6] = 3 // client abbandonato
            shared_memory[6] = 4 // client timeout
        */

        if (victory())
        {
            printf("Un giocatore ha vinto\n");
            shared_memory[6] = 1; // Vittoria
            kill(shared_memory[PID1], SIGTERM);
            kill(shared_memory[PID2], SIGTERM);
            break;
        }

        if (draw())
        {
            printf("Pareggio!\n");
            shared_memory[6] = 2; // Stato: pareggio
            kill(shared_memory[PID1], SIGTERM);
            kill(shared_memory[PID2], SIGTERM);
            break;
        }

        sleep(1);
    }

    // Rimozione memoria, semaforo
    cleanup();
    return 0;
}

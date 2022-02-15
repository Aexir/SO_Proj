#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <wait.h>

#define S1 SIGINT  /*koniec*/
#define S2 SIGUSR2 /*pauza*/
#define S3 SIGCONT /*unpause*/
#define S4 SIGALRM /*send*/

int proces1 = -1, proces2 = -1, proces3 = -1, parent, paused, sygnal, msgid;
int deskryptor1[2], deskryptor2[2], deskryptor3[2];
int wybor[2];

struct komunikat {
    long mtype;
    char teks[100];
};

struct komunikat k1;

void proces_1(); /* czyta tekst z pliku i przekazuje go dalej   */
void proces_2(); /*  Sprawdza poprawnosc tekstu z procesu 1		*/
void proces_3(); /* Pobiera tekst z procesu 2 i oblicza sume liczb    */

void sygnall(int s);
void sygnalS4(int s);
void usun_kolejke(int id_k);

void menu();

int findSum(char *str);

int pattern(char *str);

int main(int argc, char *argv[]) {
    parent = getpid();
    paused = 0;

    if (argc != 2) {
        printf("Za malo argumentow uruchamiajacych\n");
        return 0;
    }
    msgid = msgget(IPC_PRIVATE, IPC_CREAT | 0666);

    pipe(deskryptor1);
    pipe(deskryptor2);
    pipe(deskryptor3);
    pipe(wybor);

    signal(S1, sygnall);
    signal(S2, sygnall);
    signal(S3, sygnall);
    signal(S4, sygnalS4);

    /*tworzenie procesow */
    if (parent == getpid())
        proces1 = fork();
    if (parent == getpid())
        proces2 = fork();
    if (parent == getpid())
        proces3 = fork();

    /* przekazywanie sterowania odpowiednim funkcja */
    if (proces1 == 0) {
        proces_1(argv[1]);
    }
    if (proces2 == 0) {
        proces_2();
    }
    if (proces3 == 0) {
        proces_3();
    }

    if (parent == getpid()) {
        close(deskryptor1[0]);
        write(deskryptor1[1], &proces2, sizeof(proces2));
        write(deskryptor1[1], &proces3, sizeof(proces3));
        close(deskryptor1[1]);
        menu();
    }

    return 0;
}

int findSum(char *str) {
    int i, x;
    int sum = 0;
    char tmp[100];
    for (i = 0; i < 100; i++) {
        tmp[i] = '\0';
    }
    int size = 0;
    for (i = 0; i < 100; i++) {
        if (isdigit(str[i])) {
            tmp[size++] = str[i];
        } else {
            sum += atoi(tmp);
            for (x = 0; x < 100; x++) {
                tmp[x] = '\0';
            }
            size = 0;
        }
    }
    return sum;
}

int pattern(char *str) {
    int i = 0;
    if (str[0] == '+') {
        return 0;
    }
    while (str[i + 1] != '\0') {
        if (!(isdigit(str[i]) || str[i] == '+')) {
            return 0;
        }
        i++;
    }
    if (str[i] == '+') {
        return 0;
    }
    return 1;
}

void proces_1(char *nazwa_pliku) {
    char buff[100];
    int i;
    printf("(P1) %i : %i Uruchomiony!\n", getpid(), getppid());
    proces1 = getpid();
    close(deskryptor1[1]);
    read(deskryptor1[0], &proces2, sizeof(proces2));
    read(deskryptor1[0], &proces3, sizeof(proces3));

    close(deskryptor2[0]);
    write(deskryptor2[1], &proces1, sizeof(proces1));
    write(deskryptor2[1], &proces3, sizeof(proces3));
    FILE *fd = NULL;
    if ((fd = fopen(nazwa_pliku, "r")) == NULL) {
        perror("(P1) Blad otwierania pliku\n");
        exit(1);
    }
    do {
        if (fgets(buff, 100, fd) == NULL) {
            printf("Koniec pliku\n");
            printf("Usuniecie kolejek komunikatow\n");
            usun_kolejke(msgid);

            printf("Wysanie sygnalu SIGKILL do P2 i P3\n");
            kill(proces2, SIGKILL);
            kill(proces3, SIGKILL);
            kill(getppid(), SIGKILL);
            exit(1);
        }
        int dl_ciag = strlen(buff);
        buff[dl_ciag - 1] = '\0';
        k1.mtype = 1;
        memset(k1.teks, 0, 100);
        strcpy(k1.teks, buff);

        if (msgsnd(msgid, &k1, strlen(k1.teks), 0) == -1) {
            printf("(P1) Nie udalo sie wpisac komunikatu do kolejki\n");
            exit(1);
        }
        sleep(1);

        while (paused == 1) {
            pause();
        }

    } while (1);
    exit(0);
}

void usun_kolejke(int id_k) {
    if (msgctl(id_k, IPC_RMID, NULL) == -1) {
        printf("Nie moge usunac kolejki komunikatow 1 \n");
        exit(1);
    }
}

void proces_2() {
    int i;
    printf("(P2) %i : %i Uruchomiony!\n", getpid(), getppid());

    close(deskryptor2[1]);
    proces2 = getpid();
    read(deskryptor2[0], &proces1, sizeof(proces1));
    read(deskryptor2[0], &proces3, sizeof(proces3));

    close(deskryptor3[0]);
    write(deskryptor3[1], &proces1, sizeof(proces1));
    write(deskryptor3[1], &proces2, sizeof(proces2));

    do {
        memset(k1.teks, 0, 100);

        if ((msgrcv(msgid, &k1, 100, 1, MSG_NOERROR)) == -1) {
            printf("[P2] Nie udalo sie odczytac komunikatu z kolejki\n");
        } else {
            k1.mtype = 2;
            if (pattern(k1.teks)) {
                strcpy(k1.teks, k1.teks);
                if (msgsnd(msgid, &k1, strlen(k1.teks), MSG_NOERROR) == -1) {
                    printf("[P2] Nie udalo sie wpisac komunikatu do kolejki\n");
                    exit(1);
                }
            } else {
                printf("(P2) Zly format\n");
            }
        }
        while (paused == 1) {
            pause();
        }

    } while (1);

    exit(0);
}

void proces_3() {
    int i;

    printf("(P3) %i %i Uruchomiony!\n", getpid(), getppid());
    proces3 = getpid();

    close(deskryptor3[1]);
    read(deskryptor3[0], &proces1, sizeof(proces1));
    read(deskryptor3[0], &proces2, sizeof(proces2));

    do {
        memset(k1.teks, 0, 100);

        if ((msgrcv(msgid, &k1, 100, 2, MSG_NOERROR)) == -1) {
            printf("[P3] Nie udalo sie odczytac komunikatu z kolejki\n");
        } else {
            printf("(P3) Suma wyrazow: %i\n\n", findSum(k1.teks));
        }
        while (paused == 1) {
            pause();
        }

    } while (1);
    exit(0);
}

void menu() {
    int wybor_p = 0;
    int wybor_s = 0;
    int pid;

    while (wybor_s != 1) {
        printf("Witaj w panelu kontrolnym. (%i)\n", getpid());
        printf("Proces_1 (%d)\tProces_2 (%d)\tProces_3 (%d)\n", proces1, proces2, proces3);
        printf("Lista sygnalow:.\n1. S1 - Zakonczenie dzialania;\n2. S2 - Wstrzymanie dzialania;\n3. S3 - wznowienie dzialania.\n\n");
        printf("Podaj numer procesu(1-3):\n");
        scanf("%d", &wybor_p);
        printf("Podaj numer sygnalu(1-3):\n");
        scanf("%d", &wybor_s);

        switch (wybor_p) {
            case 1:
                pid = proces1;
                break;
            case 2:
                pid = proces2;
                break;
            case 3:
                pid = proces3;
                break;
            case 4:
                kill(proces1, SIGKILL);
                kill(proces2, SIGKILL);
                kill(proces3, SIGKILL);
                break;
            default:
                printf("Niepoprawny wybor procesu.\n");
                pid = -1;
                break;
        }
        if (pid > 0) {
            switch (wybor_s) {
                case 1:
                    kill(pid, SIGINT);
                    break;
                case 2:
                    kill(pid, SIGUSR2);
                    break;
                case 3:
                    kill(pid, SIGCONT);
                    break;
                default:
                    printf("Niepoprawny wybor sygnalu.\n");
                    break;
            }
        }
    }
}

void sygnall(int s) {
    printf("[%i] przechwycilem sygnal %d\n", getpid(), s);
    switch (s) {
        case S1:
            sygnal = 1;
            break;

        case S2:
            sygnal = 2;
            break;

        case S3:
            sygnal = 3;
            break;
    }
    if (write(wybor[1], &sygnal, sizeof(sygnal)) < 0)
        perror("[P1] write error: ");

    if (write(wybor[1], &sygnal, sizeof(sygnal)) < 0)
        perror("[P1] write error: ");
    if (getpid() == proces1) {
        printf("(P1:%i) Wysylam sygnaly do %d, %d\n", getpid(), proces2, proces3);

        if (kill(proces2, S4) == -1)
            printf("[P1] kill error do P2\n");

        if (kill(proces3, S4) == -1)
            printf("[P1] kill error do P3\n");
    }

    if (getpid() == proces2) {
        printf("(P2:%i) Wysylam sygnaly do %d, %d\n", getpid(), proces1, proces3);
        if (kill(proces1, S4) == -1)
            printf("[P2] kill error do P1\n");

        if (kill(proces3, S4) == -1)
            printf("[P2] kill error do P3\n");
    }

    if (getpid() == proces3) {
        printf("(P3:%i) Wysylam sygnaly do %d, %d\n", getpid(), proces1, proces2);
        if (kill(proces1, S4) == -1)
            printf("[P3] kill error do P1\n");

        if (kill(proces2, S4) == -1)
            printf("[P3] kill error do P2\n");
    }
    switch (s) {
        case S1:
            printf("%i dostalem sygnal S1, wiec koncze dzialanie\n", getpid());
            usun_kolejke(msgid);
            kill(getpid(), SIGKILL);
            break;

        case S2: {
            printf("%i dostalem sygnal S2, wiec wstrzymuje dzialanie\n", getpid());
            if (paused == 0) {
                paused = 1;
            } else {
                printf("%i byl juz zatrzymany\n", getpid());
            }
        } break;

        case S3: {
            printf("%i dostalem sygnal S1, wiec wznawiam  dzialanie\n", getpid());
            if (paused == 1) {
                paused = 0;
            } else {
                printf("%i nie byl zatrzymany\n", getpid());
            }
        }

        break;
    }
}

void sygnalS4(int s) {
    read(wybor[0], &sygnal, sizeof(sygnal));
    switch (sygnal) {
        case 1:
            printf("%i sygnal S4 - koniec dzialania\n", getpid());
            kill(getpid(), SIGKILL);
            break;
        case 2:
            if (paused == 1) {
                printf("%i Nie mozna zatrzymac zatrzymanego procesu\n", getpid());
            } else {
                printf("%i sygnal S4 - zatrzymanie\n", getpid());
                paused = 1;
            }
            break;
        case 3:

            if (paused == 0) {
                printf("%i Nie mozna wznowic dzialajacego procesu\n", getpid());
            } else {
                printf("%i sygnal S4 - wznowienie\n", getpid());
                paused = 0;
            }
            break;
    }
}
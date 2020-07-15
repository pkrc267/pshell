// C Program to design a shell in Linux
/*
*   Prakhar Yadav
*   M.E. CS 1st Year
*   Advanced Operating System
*   Assignment 2
*   2019H1030032G
*/
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <ctype.h>
//compile with: gcc psh.c -lreadline
#include <readline/readline.h>
#include <readline/history.h>
//#include <editline/readline.h>
#define PSH_TOKEN_BUFSIZE 64
#define PSH_TOKEN_DELIM " \t\r\n\a"
#define MAXLIST 100 // max number of commands to be supported
#define HISTORY_COUNT 100

//global variables
int count = 0;
char path[300];

//structure for recording history
struct hist
{
    char *cmd;
    char timestamp[100];
    bool status;
} h[HISTORY_COUNT];

//Function signatures
void psh_init();
char *psh_read();
char **psh_parse(char *);
int psh_launch(char **);
/*
  Function Declarations for builtin shell commands:
 */
int psh_num_builtins();
int psh_cd(char **);
int psh_pwd(char **);
int psh_help(char **);
int psh_exit(char **);
int psh_man(char **);
int psh_history(char **);
int psh_run(char **);

/*function declarations for implementing pipe*/
void psh_parseSpace(char *, char **);
int psh_parsePipe(char *str, char **strpiped);
void psh_execArgsPiped(char **, char **);
int psh_processString(char *, char **, char **);

/*Main program starts here-onwards*/
int main()
{
    char *path_pointer = getcwd(NULL, 1);
    strcpy(path, path_pointer);
    strcat(path, "/");
    printf("\033[H\033[J");
    printf("Welcome to PShell\n");
    printf("\nuse START command to enter the shell\nuse END command to exit rightaway\n");
    do
    {
        char *entryPoint = NULL;
        entryPoint = psh_read();
        strtok(entryPoint, "\n");
        if (strcmp(entryPoint, "START") == 0)
        {
            h[count].status = true;
            //count = (count + 1) % 10;
            ++count;
            psh_init();
        }
        else
        {
            if (strcmp(entryPoint, "END") == 0)
            {
                h[count].status = true;
                //count = (count + 1) % 10;
                ++count;
                exit(EXIT_SUCCESS);
                free(entryPoint);
            }
            else
            {
                h[count].status = false;
                //count = (count + 1) % 10;
                ++count;
                printf("\033[H\033[J");
                printf("Wrong input\nuse START to enter or END to leave pshell at this point\n");
            }
        }
        free(entryPoint);
    } while (1);

    return EXIT_SUCCESS;
}

void psh_init()
{
    char *line;
    char inputString[1000],
        *parsed[MAXLIST],
        *parsedPipe[MAXLIST];
    char **args;
    int status, execFlag;

    psh_help(NULL);
    do
    {
        printf("\npsh >> ");
        line = psh_read(); //read command inputs

        execFlag = psh_processString(line, parsed, parsedPipe);

        if (execFlag == 0)
        {
            args = psh_parse(line); //parse and store the arguments separately
            status = psh_run(args); //execute the commands and store the return status
        }
        else if (execFlag == 1)
            psh_execArgsPiped(parsed, parsedPipe);

        //free(line);
        //free(args);
    } while (status);
}

char *psh_read()
{
    char *line = malloc(100);
    char *buf;
    buf = readline(NULL);
    if (strlen(buf) != 0)
    {
        h[count].cmd = buf;
        time_t t = time(NULL);
        struct tm tm = *localtime(&t);
        char time_stat[100] = "";
        sprintf(h[count].timestamp, "%d-%d-%d, %d:%d:%d",
                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                tm.tm_hour, tm.tm_min, tm.tm_sec);
        strcpy(line, buf);
    }

    return line;
}

char **psh_parse(char *line)
{
    int bufsize = PSH_TOKEN_BUFSIZE;
    int position = 0;
    char **tokens = malloc(bufsize * sizeof(char *));
    char *token;

    if (!tokens)
    {
        fprintf(stderr, "psh: allocation error\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, PSH_TOKEN_DELIM);
    while (token != NULL)
    {
        tokens[position] = token;
        position++;

        if (position >= bufsize)
        {
            bufsize += PSH_TOKEN_BUFSIZE;
            tokens = realloc(tokens, bufsize * sizeof(char *));
            if (!tokens)
            {
                fprintf(stderr, "psh: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }

        token = strtok(NULL, PSH_TOKEN_DELIM);
    }
    tokens[position] = NULL;
    return tokens;
}

int psh_launch(char **args)
{
    pid_t pid, wpid;
    int status;

    pid = fork();
    if (pid == 0)
    {
        // Child process
        if (execvp(args[0], args) == -1)
            perror("psh");
        exit(EXIT_FAILURE);
    }
    else if (pid < 0)
    {
        // Error forking
        perror("psh");
    }
    else
    {
        // Parent process
        do
        {
            wpid = waitpid(pid, &status, WUNTRACED);
            if (!status)
            {
                h[count].status = true;
            }
            else
            {
                h[count].status = false;
            }
            //count = (count + 1) % 10;
            ++count;
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    return 1;
}

/*
  List of builtin commands, followed by their corresponding functions.
 */
char *builtin_str[] = {
    "cd",
    "pwd",
    "help",
    "man",
    "history",
    "exit"};

int (*builtin_func[])(char **) = {
    &psh_cd,
    &psh_pwd,
    &psh_help,
    &psh_man,
    &psh_history,
    &psh_exit};

int psh_num_builtins()
{
    return sizeof(builtin_str) / sizeof(char *);
}

/*
  Builtin function implementations.
*/

int psh_pwd(char **args)
{
    printf("%s\n", getcwd(NULL, 1));
    return 1;
}

int psh_cd(char **args)
{
    if (args[1] == NULL)
    {
        fprintf(stderr, "psh: expected argument to \"cd\"\n");
    }
    else
    {
        if (chdir(args[1]) != 0)
        {
            perror("psh");
        }
    }
    return 1;
}

int psh_help(char **args)
{
    int i;
    printf("Welcome to PShell\n");
    printf("Type program names and arguments, and hit enter.\n");
    printf("The following are built in:\n");

    for (i = 0; i < psh_num_builtins(); i++)
    {
        printf("  %s\n", builtin_str[i]);
    }

    printf("Use the man command for information on other programs.\n");
    return 1;
}

int psh_man(char **args)
{
    for (int i = 0; i < psh_num_builtins(); i++)
    {
        if (strcmp(args[1], builtin_str[i]) == 0)
        {
            printf("\033[H\033[J");
            //pass custom man page
            char c;
            FILE *fp;

            char local_path[300];
            strcpy(local_path, path);
            strcat(local_path, "man-pages/");
            strcat(local_path, builtin_str[i]);

            fp = fopen(local_path, "r");
            if (!fp)
            {
                printf("unable to open required man-page\n");
                return 1;
            }
            c = fgetc(fp);
            while (c != EOF)
            {
                printf("%c", c);
                c = fgetc(fp);
            }
            fclose(fp);
            return 1;
        }
    }
    return psh_launch(args);
}

/*History command modules*/
int psh_history(char **args)
{
    printf("\n");
    int k, i;
    if (count >= 10)
        i = count - 9;
    else if (count < 10)
    {
        i = 0;
    }
    for (k = i; k < count; k++)
    {
        printf(" %15s  %s  %s\n", h[k].cmd, h[k].timestamp, (h[k].status == false ? "Failure" : "Success"));
    }

    printf("\n");
    return 1;
}

int psh_exit(char **args)
{
    return 0;
}

int psh_run(char **args)
{
    int i;

    if (args[0] == NULL)
    {
        // An empty command was entered.
        return 1;
    }

    for (i = 0; i < psh_num_builtins(); i++)
    {
        if (strcmp(args[0], builtin_str[i]) == 0)
        {
            int x = (*builtin_func[i])(args);
            h[count].status = true;
            count = (count + 1) % 10;
            return x;
        }
    }

    return psh_launch(args);
}

/* Functions for implementing pipes*/

int psh_processString(char *str, char **parsed, char **parsedpipe)
{

    char *strpiped[2];
    int piped = 0;

    piped = psh_parsePipe(str, strpiped);

    if (piped)
    {
        psh_parseSpace(strpiped[0], parsed);
        psh_parseSpace(strpiped[1], parsedpipe);
    }
    if (piped == 1)
        return 1;
    else
        return 0;
}

// function for finding pipe
int psh_parsePipe(char *str, char **strpiped)
{
    int i;
    for (i = 0; i < 2; i++)
    {
        strpiped[i] = strsep(&str, "|");
        if (strpiped[i] == NULL)
            break;
    }

    if (strpiped[1] == NULL)
        return 0; // returns zero if no pipe is found.
    else
        return 1;
}

// function for parsing command words
void psh_parseSpace(char *str, char **parsed)
{
    int i;

    for (i = 0; i < MAXLIST; i++)
    {
        parsed[i] = strsep(&str, " ");

        if (parsed[i] == NULL)
            break;
        if (strlen(parsed[i]) == 0)
            i--;
    }
}

// Function where the piped system commands is executed
void psh_execArgsPiped(char **parsed, char **parsedpipe)
{
    // 0 is read end, 1 is write end
    int pipefd[2];
    pid_t p1, p2;

    if (pipe(pipefd) < 0)
    {
        printf("\nPipe could not be initialized");
        return;
    }
    p1 = fork();
    if (p1 < 0)
    {
        printf("\nCould not fork");
        return;
    }

    if (p1 == 0)
    {
        // Child 1 executing..
        // It only needs to write at the write end
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        if (execvp(parsed[0], parsed) < 0)
        {
            printf("\nCould not execute command 1..");
            exit(0);
        }
    }
    else
    {
        // Parent executing
        p2 = fork();

        if (p2 < 0)
        {
            printf("\nCould not fork");
            return;
        }

        // Child 2 executing..
        // It only needs to read at the read end
        if (p2 == 0)
        {
            close(pipefd[1]);
            dup2(pipefd[0], STDIN_FILENO);
            close(pipefd[0]);
            if (execvp(parsedpipe[0], parsedpipe) < 0)
            {
                printf("\nCould not execute command 2..");
                exit(0);
            }
        }
        else
        {
            // parent executing, waiting for two children
            wait(NULL);
            wait(NULL);
        }
    }
}
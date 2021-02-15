#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "chunk.h"
#include "debug.h"
#include "vm.h"
#include "ansi-color.h"

#define VM_VERSION "1.0.0-alpha"

static char *trimwhitespace(char *str)
{
    char *end;
    while (isspace((unsigned char)*str))
        str++;
    if (*str == 0)
        return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end))
        end--;
    end[1] = '\0';
    return str;
}

static void runFromREPL()
{
    for (;;)
    {
        char *userInput = readline("meon > ");
        if (userInput == NULL)
        {
            printf("\n");
            break;
        }
        char *code = trimwhitespace(userInput);
        if (strcmp(code, "") == 0)
        {
            continue;
        }
        add_history(code);
        interpret(code, "REPL");
    }
}

static char *readFile(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL)
    {
        fprintf(stderr, RED "\nError: cannot OPEN '%s'. Possible error: ENOENT, EACCES.\n\n" RESET, path);
        exit(74);
    }

    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    char *buffer = (char *)malloc(fileSize + 1);
    if (buffer == NULL)
    {
        fprintf(stderr, RED "\nError: not ENOUGH MEMORY to read '%s'.\n\n", path);
        exit(74);
    }
    size_t b_read = fread(buffer, sizeof(char), fileSize, file);
    if (b_read < fileSize)
    {
        fprintf(stderr, "\nError: could not READ '%s'\n\n", path);
        exit(74);
    }
    buffer[b_read] = '\0';

    fclose(file);
    return buffer;
}

static void runFromFile(const char *path)
{
    char *source = readFile(path);
    InterpretResult result = interpret(source, path);
    free(source);

    if (result == INTERPRET_COMPILE_ERROR)
        exit(65);
    if (result == INTERPRET_RUNTIME_ERROR)
        exit(70);
}

static void showInterpreterInfo(int exitStatus, bool shouldExit)
{
#define FD ((exitStatus) == 0 ? stdout : stderr)
    fprintf(FD, "\n");
    fprintf(FD, GRN "Meon VM " RESET "version " YEL VM_VERSION RESET " ( built on: " __TIMESTAMP__ " )\n");
    fprintf(FD, "(c) " YEL "2021 - present" RESET " Hein Thant Maung Maung. Licensed under " YEL "MIT.\n" RESET);
    fprintf(FD, "\n");
#undef FD
    if (shouldExit)
        exit(exitStatus);
}

static void showUsage(int exitStatus)
{
#define FD ((exitStatus) == 0 ? stdout : stderr)
    showInterpreterInfo(exitStatus, false);

    fprintf(FD, YEL "SYNOPSIS:\n\n" RESET);
    fprintf(FD, "    meon [command] [arguments]\n\n");
    fprintf(FD, YEL "COMMANDS:\n\n" RESET);
    fprintf(FD, GRN "    -h, --help" RESET "\t\tShow Usage information like this.\n");
    fprintf(FD, GRN "    -v, --version" RESET "\tShow VM version information.\n");
    fprintf(FD, GRN "    -c, --compile" RESET "\tCompile Meon to native executable. (beta).\n");
    fprintf(FD, GRN "    -r, --run" RESET "\t\tInterpret and evaluate Meon. (beta).\n");
    fprintf(FD, "\n");
    fprintf(FD, YEL "EXAMPLES:\n\n" RESET);
    fprintf(FD, GRN "    meon -r hello.meon" RESET "\tInterpret and evaluate 'hello.meon'.\n");
    fprintf(FD, GRN "    meon -c hello.meon" RESET "\tCompile 'hello.meon' to native executable (beta).\n");
    fprintf(FD, "\n");
#undef FD
    exit(exitStatus);
}

int main(int argc, char *argv[])
{
    initVM();

    if (argc == 1)
    {
        runFromREPL();
    }
    else if (argc == 2)
    {
        const char *option = argv[1];
        if (strcmp(option, "-h") == 0 || strcmp(option, "--help") == 0)
        {
            showUsage(0);
        }
        else if (strcmp(option, "-v") == 0 || strcmp(option, "--version") == 0)
        {
            showInterpreterInfo(0, true);
        }
        else
        {
            showUsage(1);
        }
    }
    else if (argc == 3)
    {
        const char *option = argv[1];
        const char *file = argv[2];

        if (strcmp(option, "-r") == 0 || strcmp(option, "--run") == 0)
        {
            runFromFile(file);
        }
        else if (strcmp(option, "-c") == 0 || strcmp(option, "--compile") == 0)
        {
            printf(YEL "\nIt's under development. Please come back later.\n\n" RESET);
        }
        else
        {
            showUsage(1);
        }
    }
    else
    {
        showUsage(1);
    }
    freeVM();
    return 0;
}

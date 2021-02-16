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
        interpret(code, "REPL", 0);
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

static void runFromFile(const char *path, int debugLevel)
{
    char *source = readFile(path);
    InterpretResult result = interpret(source, path, debugLevel);
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
    fprintf(FD, "    meon [command] [option] [arguments]\n\n");
    fprintf(FD, YEL "COMMANDS:\n\n" RESET);
    fprintf(FD, GRN "    -h, --help" RESET "\t\tShow Usage information like this.\n");
    fprintf(FD, GRN "    -v, --version" RESET "\tShow VM version information.\n");
    fprintf(FD, GRN "    -r, --run" RESET "\t\tInterpret and evaluate Meon. (beta).\n");
    fprintf(FD, "\n");
    fprintf(FD, YEL "OPTIONS:\n\n" RESET);
    fprintf(FD, GRN "    -d, --disassemble" RESET "\t\tRun interpreter and also show disassembled instructions.\n");
    fprintf(FD, GRN "    -dd, --debug" RESET "\tRun interpreter and also show disassembled instructions and execution trace.\n");
    fprintf(FD, "\n");
    fprintf(FD, YEL "EXAMPLES:\n\n" RESET);
    fprintf(FD, GRN "    meon -r hello.meon" RESET "\tInterpret and evaluate 'hello.meon'.\n");
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
    else if (argc > 1 && argc < 5)
    {
        const char *command = argv[1];
        if (strcmp(command, "-h") == 0 || strcmp(command, "--help") == 0)
        {
            showUsage(0);
        }
        else if (strcmp(command, "-v") == 0 || strcmp(command, "--version") == 0)
        {
            showInterpreterInfo(0, true);
        }
        else if (strcmp(command, "-r") == 0 || strcmp(command, "--run") == 0)
        {
            const char *option = argv[2];
            if (option == NULL)
                showUsage(1);

            int debugLevel = 0;

            if (strcmp(option, "-d") == 0 || strcmp(option, "--disassemble") == 0)
            {
                debugLevel = 1;
                const char *file = argv[3];
                if (file == NULL)
                    showUsage(1);
                runFromFile(file, debugLevel);
            }
            else if (strcmp(option, "-dd") == 0 || strcmp(option, "--debug") == 0)
            {
                debugLevel = 2;
                const char *file = argv[3];
                if (file == NULL)
                    showUsage(1);
                runFromFile(file, debugLevel);
            }
            else
            {
                const char *lateOption = argv[3];
                if (lateOption != NULL)
                {
                    if (strcmp(lateOption, "-d") == 0 || strcmp(lateOption, "--disassemble") == 0)
                    {
                        debugLevel = 1;
                    }
                    else if (strcmp(lateOption, "-dd") == 0 || strcmp(lateOption, "--debug") == 0)
                    {
                        debugLevel = 2;
                    }
                    else
                    {
                        showUsage(1);
                    }
                }
                runFromFile(option, debugLevel);
            }
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

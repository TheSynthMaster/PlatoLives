#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "plato/console_runner.h"
#include "plato/plato_script.h"

static void print_help(const char *progname) {
    printf("PlatoLives v4.5 - CDC PLATO IV & Cyber1 Terminal Emulator\n\n");
    printf("Usage:\n  %s [options] [host] [port]\n\n", progname);
    printf("Options:\n");
    printf("  --script <file>        Execute a PLATO script autonomously (headless)\n");
    printf("  --script -h            Display PLATO Scripting Engine language reference\n");
    printf("  --test-script <file>   Alias for --script (backward compatibility)\n");
    printf("  --version, -v          Display application version and exit\n");
    printf("  --help, -h             Display this help message and exit\n\n");
    printf("Default host:port is cyberserv.org:8005\n");
}

int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "--script") == 0 || strcmp(argv[i], "--test-script") == 0) &&
            i + 1 < argc && (strcmp(argv[i+1], "-h") == 0 || strcmp(argv[i+1], "--help") == 0 || strcmp(argv[i+1], "help") == 0)) {
            printf("%s\n", plato_script_get_manual_text());
            return 0;
        }
        if (strcmp(argv[i], "--script-help") == 0) {
            printf("%s\n", plato_script_get_manual_text());
            return 0;
        }
    }

    const char *host = NULL;
    int port = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--v") == 0 || strcmp(argv[i], "-V") == 0) {
            printf("PlatoLives v4.5\n");
            return 0;
        }
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--h") == 0) {
            print_help(argv[0]);
            return 0;
        }
        if (strcmp(argv[i], "--script") == 0 || strcmp(argv[i], "--test-script") == 0) {
            if (i + 1 >= argc || argv[i + 1][0] == '-') {
                fprintf(stderr, "[PlatoLives] Error: %s requires a script file path\n", argv[i]);
                return 1;
            }
            const char *script_file = argv[i + 1];
            for (int j = i + 2; j < argc; j++) {
                if (argv[j][0] != '-') {
                    if (!host) host = argv[j];
                    else if (port == 0) port = atoi(argv[j]);
                }
            }
            return plato_script_run_file(script_file, host, port);
        }
        if (argv[i][0] != '-') {
            if (!host) {
                host = argv[i];
            } else if (port == 0) {
                port = atoi(argv[i]);
            }
        }
    }

    return plato_console_run(host, port);
}

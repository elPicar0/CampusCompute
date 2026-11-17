#ifndef CLI_H
#define CLI_H

#define EXIT_OK 0
#define EXIT_RUNTIME 1   /* a feature ran but failed / bad input file */
#define EXIT_USAGE 2     /* unknown command or malformed arguments    */

/* Dispatches argv[1] to a command handler. Returns the process exit code. */
int cliRun(int argc, char** argv);

void cliPrintUsage(void);

#endif

/* envdump_z089.c - Z-089 environment dumper for the
 * one-variable proof.
 *
 * Prints the process environment (the raw environ
 * array) using ONLY write(2): no malloc, no stdio, no
 * allocation of any kind. That is the whole point: the
 * dumper must run truthfully INSIDE the poisoned
 * process, where every allocation fails. The obvious
 * tool cannot do this job - on this host /usr/bin/env
 * allocates at startup and died under the poison in the
 * drill's first run ("memory allocation of 24 bytes
 * failed"), which is itself filed evidence of how
 * total the poison's breakage is. Sorting happens in a
 * separate unpoisoned process downstream of the pipe;
 * the LD_PRELOAD applies only to this dumper's exec. */
#include <unistd.h>

extern char **environ;

int main(void)
{
    char **e;
    for (e = environ; e != NULL && *e != NULL; e++) {
        const char *s = *e;
        size_t n = 0;
        while (s[n] != '\0')
            n++;
        if (write(1, s, n) < 0 || write(1, "\n", 1) < 0)
            return 1;
    }
    return 0;
}

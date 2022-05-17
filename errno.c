/*
 * errno.c -- look up errno names and descriptions
 * Copyright 2012 Lars Wirzenius (liw@iki.fi)
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>


static struct {
    const char *name;
    int code;
} errnos[] = {
    #include "errnos.h"
};
static const int num_errnos = sizeof(errnos) / sizeof(errnos[0]);


static void
report(const char *name, int code)
{
    printf("%s %d %s\n", name, code, strerror(code));
}


static bool
report_from_name(const char *name)
{
    int i;
    for (i = 0; i < num_errnos; ++i) {
        if (strcasecmp(errnos[i].name,  name) == 0) {
            report(errnos[i].name, errnos[i].code);
            return true;
        }
    }
    return false;
}


static bool
report_from_code(int code)
{
    int i;
    for (i = 0; i < num_errnos; ++i) {
        if (errnos[i].code == code) {
            report(errnos[i].name, code);
            return true;
        }
    }
    return false;
}


static bool
matches(int code, int num_words, char **words)
{
    const char *text = strerror(code);
    int i;
    
    for (i = 0; i < num_words; ++i) {
        if (strcasestr(text, words[i]) == NULL)
            return false;
    }
    return true;
}


static void
search(int num_words, char **words)
{
    int i;
    
    for (i = 0; i < num_errnos; ++i) {
        if (matches(errnos[i].code, num_words, words))
            report(errnos[i].name, errnos[i].code);
    }
}


static void
search_all(int num_words, char **words)
{
    FILE *f;
    
    /* Static buffers are ugly, but they're simple. If anyone has a 
       locale name longer than a kilobyte, they will suffer, and they
       will complain, and then I will fix this. */
    char line[1024];

    f = popen("locale -a", "r");
    if (f == NULL) {
        fprintf(stderr, "ERROR: Can't execute locale -a: %d: %s\n",
                errno, strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    while (fgets(line, sizeof line, f) != NULL) {
        line[strcspn(line, "\n")] = '\0';
        setlocale(LC_ALL, line);
        search(num_words, words);
    }

    pclose(f);
}


static struct option
options[] = {
    { "help", 0, NULL, 'h' },
    { "list", 0, NULL, 'l' },
    { "search", 0, NULL, 's' },
    { "search-all-locales", 0, NULL, 'S' },
    { 0, 0, 0, 0 }
};


static void
usage(void)
{
    printf("Usage: errno [-lsS] [--list] [--search] [--search-all-locales] "
           "[keyword]\n");
}


int 
main(int argc, char **argv)
{
    int i;
    int exit_code;
    int index = 0;
    enum {
        lookup_mode, 
        list_mode, 
        search_mode, 
        search_all_mode 
    } mode = lookup_mode;
    
    setlocale(LC_ALL, "");
    
    for (;;) {
        int c = getopt_long(argc, argv, "hlsS", options, &index);
        if (c == -1)
            break;
            
        switch (c) {
        case 'h':
            usage();
            return EXIT_SUCCESS;

        case 'l':
            mode = list_mode;
            break;
            
        case 's':
            mode = search_mode;
            break;
            
        case 'S':
            mode = search_all_mode;
            break;

        case '?':
            break;

        default:
            fprintf(stderr, "getopt returned 0x%02x\n", c);
            return EXIT_FAILURE;
        }
    }
 
    exit_code = EXIT_SUCCESS;

    switch (mode) {
    case lookup_mode:
        for (i = optind; i < argc; ++i) {
            const char *arg = argv[i];
            if (toupper(arg[0]) == 'E') {
                if (!report_from_name(arg))
                    exit_code = EXIT_FAILURE;
            } else if (isdigit(arg[0])) {
                if (!report_from_code(atoi(arg)))
                    exit_code = EXIT_FAILURE;
            } else {
                fprintf(stderr, "ERROR: Not understood: %s\n", arg);
                exit_code = EXIT_FAILURE;
            }
        }
        break;
        
    case list_mode:
        for (i = 0; i < num_errnos; ++i)
            report(errnos[i].name, errnos[i].code);
        break;
        
    case search_mode:
        search(argc - optind, argv + optind);
        break;
        
    case search_all_mode:
        search_all(argc - optind, argv + optind);
        break;
    }

    return exit_code;
}

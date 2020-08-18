/*
 *  parallel.c - run commands in parallel until you run out of commands
 *
 *  Copyright © 2008  Tollef Fog Heen <tfheen@err.no>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  version 2 as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 *
 */

#define _GNU_SOURCE
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

#ifdef __sun
# include <sys/loadavg.h> /* getloadavg() */
#endif

#if !defined(WEXITED)
#define WEXITED 0
#endif

static pid_t pipe_child_stdout = 0;
static pid_t pipe_child_stderr = 0;

void usage() {
	printf("parallel [OPTIONS] command -- arguments\n\tfor each argument, "
	       "run command with argument, in parallel\n");
	printf("parallel [OPTIONS] -- commands\n\trun specified commands in parallel\n");
	exit(1);
}

static void redirect(int fd, int target_fd, const char *name)
{
	if (fd == target_fd)
		return;

	if (dup2(fd, target_fd) < 0) {
		fprintf(stderr, "unable to open %s from internal pipe: %s\n",
			name, strerror(errno));
		exit(1);
	}
	close(fd);
}

void exec_child(char **command, char **arguments, int replace_cb, int nargs,
		int stdout_fd, int stderr_fd)
{
	if (fork() != 0) {
		return;
	}

	redirect(stdout_fd, 1, "stdout");
	redirect(stderr_fd, 2, "stderr");

	if (command[0]) {
		char **argv;
		int argc = 0;
		int i;
		char *s;

		while (command[argc] != 0) {
			argc++;
		}
		if (! replace_cb)
			argc++;
		argv = calloc(sizeof(char*), argc + nargs);

		for (i = 0; i < argc; i++) {
			while (replace_cb && (s=strstr(command[i], "{}"))) {
				char *buf=malloc(strlen(command[i]) + strlen(arguments[0]));
				s[0]='\0';
				sprintf(buf, "%s%s%s", command[i], arguments[0], s+2);
				command[i]=buf;
			}
			argv[i] = command[i];
		}
		if (! replace_cb)
			memcpy(argv + i - 1, arguments, nargs * sizeof(char *));
		execvp(argv[0], argv);
		exit(1);
	}
	else {
		int ret=system(arguments[0]);
		if (WIFEXITED(ret)) {
			exit(WEXITSTATUS(ret));
		}
		else {
			exit(1);
		}
	}
	return;
}

#if defined(__CYGWIN__)
typedef enum {
	P_ALL,
	P_PID,
	P_PGID
} idtype_t;
int waitid(idtype_t idtype, id_t id, siginfo_t *infop, int options) {
	pid_t pid;
	switch (idtype) {
		case P_PID:
			pid = id;
			break;
		case P_PGID:
			pid = -id;
			break;
		case P_ALL:
			pid = -1;
			break;
		default:
			errno = EINVAL;
			return -1;
	}
	int status;
	pid = waitpid(pid, &status, WEXITED | options);
	if (pid == -1) {
		return -1;
	}
	infop->si_pid = pid;
	infop->si_signo = SIGCHLD;
	if (WIFEXITED(status)) {
		infop->si_code = CLD_EXITED;
		infop->si_status = WEXITSTATUS(status);
	}
	else if (WIFSIGNALED(status)) {
		infop->si_code = CLD_KILLED;
		infop->si_status = WTERMSIG(status);
#ifdef WCOREDUMP
		if (WCOREDUMP(status)) {
			infop->si_code = CLD_DUMPED;
		}
#endif
	}
	else if (WIFSTOPPED(status)) {
		infop->si_code = CLD_STOPPED;
		infop->si_status = WSTOPSIG(status);
	}
	else if (WIFCONTINUED(status)) {
		infop->si_code = CLD_CONTINUED;
		infop->si_status = SIGCONT;
	}
	return 0;
}
#endif

int wait_for_child(int options) {
	id_t id_ignored = 0;
	siginfo_t infop;

	infop.si_pid = 0;
	waitid(P_ALL, id_ignored, &infop, WEXITED | options);
	if (infop.si_pid == 0) {
		return -1; /* Nothing to wait for */
	}
	if (infop.si_code == CLD_EXITED) {
		return infop.si_status;
	}
	return 1;
}

static int pipe_child(int fd, int orig_fd)
{
	const char *fd_info = (orig_fd == 1) ? "out" : "err";
	char buf[4096];
	int r;

	while ((r = read(fd, buf, sizeof(buf))) >= 0) {
		int w;
		int len;

		len = r;

		do {
			w = write(orig_fd, buf, len);
			if (w < 0) {
				fprintf(stderr, "unable to write to std%s: "
					"%s\n", fd_info, strerror(errno));
				exit(1);
			}

			len -= w;
		} while (len > 0);
	}

	fprintf(stderr, "unable to read from std%s: %s\n", fd_info,
		strerror(errno));
	exit(1);
}

pid_t create_pipe_child(int *fd, int orig_fd)
{
	int fds[2];
	pid_t pid;

	if (pipe(fds)) {
		fprintf(stderr, "unable to create pipe: %s\n",
			strerror(errno));
		exit(1);
	}

	*fd = fds[1];

	pid = fork();
	if (pid < 0) {
		fprintf(stderr, "unable to fork: %s\n", strerror(errno));
		return pid;
	}

	if (pid) {
		close(fds[0]);
		return pid;
	}

	close(fds[1]);

	return pipe_child(fds[0], orig_fd);
}

#if defined(__CYGWIN__) || defined(__UCLIBC__)
int getloadavg(double loadavg[], int nelem) {
	int fd, n, elem;
	char buf[128];
	char const* p = buf;
	fd = open("/proc/loadavg", O_RDONLY);
	if (fd == -1) {
		return -1;
	}
	n = read(fd, buf, sizeof(buf)-1);
	if (close(fd) == -1 || n == -1) {
		return -1;
	}
	buf[n] = '\0';
	for (elem = 0; elem < nelem; elem++) {
		char* end;
		double d = strtod(p, &end);
		if (p == end) {
			break;
		}
		loadavg[elem] = d;
		p = end;
	}
	return elem == 0 ? -1 : elem;
}
#endif

int main(int argc, char **argv) {
	int maxjobs = -1;
	int curjobs = 0;
	double maxload = -1;
	int argsatonce = 1;
	int opt;
	char **command = calloc(sizeof(char*), argc);
	char **arguments = NULL;
	int argidx = 0;
	int arglen = 0;
	int cidx = 0;
	int returncode = 0;
	int replace_cb = 0;
	int stdout_fd = 1;
	int stderr_fd = 2;
	char *t;

	while ((argv[optind] && strcmp(argv[optind], "--") != 0) &&
	       (opt = getopt(argc, argv, "+hij:l:n:")) != -1) {
		switch (opt) {
		case 'h':
			usage();
			break;
		case 'i':
			replace_cb = 1;
			break;
		case 'j':
			errno = 0;
			maxjobs = strtoul(optarg, &t, 0);
			if (errno != 0 || (t-optarg) != strlen(optarg)) {
				fprintf(stderr, "option '%s' is not a number\n",
					optarg);
				exit(2);
			}
			break;
		case 'l':
			errno = 0;
			maxload = strtod(optarg, &t);
			if (errno != 0 || (t-optarg) != strlen(optarg)) {
				fprintf(stderr, "option '%s' is not a number\n",
					optarg);
				exit(2);
			}
			break;
		case 'n':
			errno = 0;
			argsatonce = strtoul(optarg, &t, 0);
			if (errno != 0 || argsatonce < 1 || (t-optarg) != strlen(optarg)) {
				fprintf(stderr, "option '%s' is not a positive number\n",
					optarg);
				exit(2);
			}
			break;
		default: /* ’?’ */
			usage();
			break;
		}
	}
	
	if (replace_cb && argsatonce > 1) {
		fprintf(stderr, "options -i and -n are incompatible\n");
		exit(2);
	}

	if (maxjobs < 0) {
#ifdef _SC_NPROCESSORS_ONLN
		maxjobs = sysconf(_SC_NPROCESSORS_ONLN);
#else
#warning Cannot autodetect number of CPUS on this system: _SC_NPROCESSORS_ONLN not defined.
		maxjobs = 1;
#endif
	}
	
	while (optind < argc) {
		if (strcmp(argv[optind], "--") == 0) {
			int i;

			optind++;
			arglen = argc - optind;
			arguments = calloc(sizeof(char *), arglen);
			if (! arguments) {
				exit(1);
			}

			for (i = 0; i < arglen; i++) {
				arguments[i] = strdup(argv[optind + i]);
			}
			optind += i;
		}
		else {
			command[cidx] = strdup(argv[optind]);
			cidx++;
		}
		optind++;
	}

	if (argsatonce > 1 && ! command[0]) {
		fprintf(stderr, "option -n cannot be used without a command\n");
		exit(2);
	}

	pipe_child_stdout = create_pipe_child(&stdout_fd, 1);
	pipe_child_stderr = create_pipe_child(&stderr_fd, 2);

	if ((pipe_child_stdout < 0) || (pipe_child_stderr < 0))
		exit(1);

	while (argidx < arglen) {
		double load;

		getloadavg(&load, 1);

		if ((maxjobs == 0 || curjobs < maxjobs) &&
		    (maxload < 0 || load < maxload)) {

			if (argsatonce > arglen - argidx)
				argsatonce = arglen - argidx;
			exec_child(command, arguments + argidx,
				   replace_cb, argsatonce, stdout_fd,
				   stderr_fd);
			argidx += argsatonce;
			curjobs++;
		}
		
		if (maxjobs == 0 || curjobs == maxjobs) {
			returncode |= wait_for_child(0);
			curjobs--;
		}

		if (maxload > 0 && load >= maxload) {
			int r;
			sleep(1); /* XXX We should have a better
				   * heurestic than this */
			r = wait_for_child(WNOHANG);
			if (r > 0)
				returncode |= r;
			if (r != -1)
				curjobs--;
		}
	}
	while (curjobs > 0) {
		returncode |= wait_for_child(0);
		curjobs--;
	}

	if (pipe_child_stdout) {
		kill(pipe_child_stdout, SIGKILL);
		wait_for_child(0);
	}
	if (pipe_child_stderr) {
		kill(pipe_child_stderr, SIGKILL);
		wait_for_child(0);
	}

	return returncode;
}


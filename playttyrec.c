/*
 * Copyright (c) 2003-2004, Jilles Tjoelker
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with
 * or without modification, are permitted provided that the
 * following conditions are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the
 *    following disclaimer.
 * 2. Redistributions in binary form must reproduce the
 *    above copyright notice, this list of conditions and
 *    the following disclaimer in the documentation and/or
 *    other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */
/*
 * playttyrec 1.0
 * An improved ttyrec player, conceived for NetHack playback.
 * Some keys are available during playback:
 * s         - pause playback until key pressed
 * q         - quit
 * space     - abort current delay and continue immediately
 * b         - go to previous screen clear
 * f         - go to next screen clear
 * .         - toggle no-delay playback (*very* fast)
 * /         - search for a string (screen may mess up a little)
 * g         - go to beginning
 * m<letter> - mark a position
 * '<letter> - go to mark
 */
/*
 * Compilation:
 * The program has been tested on FreeBSD, Linux and Solaris.
 * FreeBSD/Linux:
 * Simply compile with something like 'make playttyrec' or
 * 'gcc -W -Wall -o playttyrec playttyrec.c'.
 * Solaris:
 * You need some implementation of err() and similar functions.
 * I compile with 'cc playttyrec.c -o playttyrec
 * -I/usr/local/include -L/usr/local/lib -lbsderr' where
 * /usr/local/lib/libbsderr.a is an err implementation taken from
 * NetBSD pkgsrc ftp (pkgtools/libnbcompat was not tried).
 * Both gcc and Sun cc work.
 */

#include	<sys/types.h>
#include	<sys/time.h>

#include	<ctype.h>
#include	<err.h>
#include	<errno.h>
#include	<fcntl.h>
#include	<signal.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<termios.h>
#include	<unistd.h>

struct termios origT, progT;

void fatal_signal(int sig);
void cleanup_term(char *message);

void fatal_signal(int sig)
{

    (void)sig;
    cleanup_term("Signal\n");
}

void
cleanup_term(char *message)
{
    char *e;

    write(STDOUT_FILENO, "\017\033[H\033[J", 7);
    if (message != NULL && *message != '\0')
    {
	if (*message == '@')
	{
	    message++;
	    write(STDOUT_FILENO, message, strlen(message));
	    e = strerror(errno);
	    write(STDOUT_FILENO, e, strlen(e));
	    write(STDOUT_FILENO, "\n", 1);
	}
	else
	    write(STDOUT_FILENO, message, strlen(message));
    }
    tcsetattr(STDOUT_FILENO, TCSANOW, &origT);
    exit(message != NULL ? 1 : 0);
}

int inputstr(char *buf, int buflen)
{
    int n = 0;
    char c;

    for (;;)
    {
	if (read(STDOUT_FILENO, &c, 1) != 1)
	    return 0;
	if (c == 8 || c == 127)
	{
	    if (n > 0)
	    {
		n--;
		write(STDOUT_FILENO, "\010 \010", 3);
	    }
	    else
		return 0;
	    buf[n] = 0;
	}
	else if (c == 21 || c == 24)
	{
	    while (n > 0)
	    {
		n--;
		write(STDOUT_FILENO, "\010 \010", 3);
	    }
	    buf[n] = 0;
	}
	else if (c == 10 || c == 13)
	{
	    /* Erase on display */
	    while (n > 0)
	    {
		n--;
		write(STDOUT_FILENO, "\010 \010", 3);
	    }
	    /* Tricky, keep old string if Enter pressed as first char */
	    return 1;
	}
	else if (isprint(c) && n < buflen)
	{
	    buf[n] = c;
	    n++;
	    buf[n] = '\0';
	    write(STDOUT_FILENO, &c, 1);
	}
    }
    return 0;
}

int
main(int argc, char *argv[])
{
    unsigned char buf[12];
    char data[65536];
    FILE *input;
    int n,r;
    struct timeval prev, cur, diff, thres;
    int first = 1;
    int do_delay = 1, do_output = 1, do_search = 0, wait_for_next_screen = 0;
    int isclrscr, had_to_output;
    int marknum;
    char c;
    char searchstr[256] = "";
    off_t o = 0, *clrscroffsets, marks[26] = { 0 }, quick_until = -1;
    int clrscroffsetallocated = 1000, cscount = 0, cscur = 0;
    fd_set FRFDS, RFDS;

    thres.tv_sec  = 5;
    thres.tv_usec = 0;

    clrscroffsets = malloc(clrscroffsetallocated * sizeof(*clrscroffsets));
    if (clrscroffsets == NULL)
	err(1, "malloc");

    if (argc != 2)
    {
	fprintf(stderr, "Usage: %s file\n", argv[0]);
	return 1;
    }

    input = fopen(argv[1], "r");
    if (input == NULL)
	err(1, "cannot open %s", argv[1]);

    if (!isatty(STDOUT_FILENO))
	errx(1, "stdout must be a terminal");

    if (tcgetattr(STDOUT_FILENO, &origT) == -1)
	err(1, "tcgetattr");
    signal(SIGINT, fatal_signal);
    signal(SIGTERM, fatal_signal);
    progT = origT;
    progT.c_lflag &= ~(ECHO | ICANON);
    progT.c_cc[VMIN] = 1; /* Solaris box I tested on has 4 by default. */
    progT.c_cc[VTIME] = 0;
    if (tcsetattr(STDOUT_FILENO, TCSANOW, &progT) == -1)
	err(1, "tcsetattr");

    FD_ZERO(&FRFDS);
    FD_SET(STDOUT_FILENO, &FRFDS);

    while ((errno = 0), (r = fread(buf, 1, 12, input)) >= 0)
    {
	if (r == 12)
	{
	    cur.tv_sec = (((((buf[3] << 8) | buf[2]) << 8) | buf[1]) << 8) | buf[0];
	    cur.tv_usec = (((((buf[7] << 8) | buf[6]) << 8) | buf[5]) << 8) | buf[4];
	    if (first)
	    {
		first = 0;
	    }
	    else
	    {
		diff.tv_sec = cur.tv_sec - prev.tv_sec;
		diff.tv_usec = cur.tv_usec - prev.tv_usec;
		if (diff.tv_usec < 0)
		{
		    diff.tv_usec += 1000000;
		    diff.tv_sec--;
		}
		if (timercmp(&diff, &thres, >))
		  diff = thres;
	    }
	    prev = cur;
	    n = (((((buf[11] << 8) | buf[10]) << 8) | buf[9]) << 8) | buf[8];
	    if ((unsigned int)n >= sizeof(data)) /* keep space for '\0' */
		cleanup_term("h->len too big\n");
	    r = fread(data, 1, n, input);
	    if (n != r)
		cleanup_term("@read input: ");

	    data[r] = '\0';
	    isclrscr = o == 0 || strstr(data, "\033[2J") != NULL;
	    if (cscount == 0 || (o > clrscroffsets[cscount - 1] && isclrscr))
	    {
		if (cscount > clrscroffsetallocated)
		{
		    clrscroffsetallocated *= 2;
		    clrscroffsets = realloc(clrscroffsets,
			    clrscroffsetallocated * sizeof(*clrscroffsets));
		    if (clrscroffsets == NULL)
			/* forget about dangling memory */
			cleanup_term("realloc failed");
		}
		clrscroffsets[cscount++] = o;
	    }
	    o += 12 + r;

	    had_to_output = do_output;

	    if (do_search && strstr(data, searchstr) != NULL)
	    {
		quick_until = o;
		cscur = cscount - 1;
		while (o <= clrscroffsets[cscur] && cscur > 0)
		    cscur--;
		o = clrscroffsets[cscur];
		fseeko(input, o, SEEK_SET);
		do_search = 0;
		do_output = 1;
		do_delay = 0;
		diff.tv_sec = diff.tv_usec = 0;
	    }
	    if (wait_for_next_screen && isclrscr)
	    {
		wait_for_next_screen = 0;
		do_delay = do_output = had_to_output = 1;
		diff.tv_sec = diff.tv_usec = 0;
	    }
	    if (quick_until != -1 && o >= quick_until)
	    {
		quick_until = -1;
		diff.tv_sec = diff.tv_usec = 0;
		do_delay = 1;
	    }
	}
	else if (r != 0)
	    break;
	else
	{
	    exit(0);
	    write(STDOUT_FILENO, "[EOF]", 5);
	    clearerr(input);
	}

	RFDS = FRFDS;
	if (!do_delay)
	    diff.tv_sec = diff.tv_usec = 0;
	if (select(STDOUT_FILENO + 1, &RFDS, NULL, NULL, r == 0 ? NULL : &diff)
		== 1)
	{
	    read(STDOUT_FILENO, &c, 1);
	    switch (c)
	    {
		case 'q': case 'Q':
		    cleanup_term(NULL);
		    exit(0);
		    break;
		case 'p': case 'P':
		    read(STDOUT_FILENO, &c, 1);
		    do_search = 0;
		    do_delay = do_output = 1;
		    /*do_search = delay_on_next_screen = 0;*/
		    break;
		case ' ':
		    break;
		case '.':
		    do_delay = !do_delay;
		    do_search = wait_for_next_screen = 0;
		    break;
		case 'f': case 'F':
		    do_delay = do_search = do_output = 0;
		    wait_for_next_screen = 1;
		    break;
		case 'b': case 'B':
		    cscur = cscount - 1;
		    o -= 12 + r;
		    if (o < 0)
			o = 0;
		    while (o <= clrscroffsets[cscur] && cscur > 0)
			cscur--;
		    if (cscur > 0)
			cscur--;
		    o = clrscroffsets[cscur];
		    fseeko(input, o, SEEK_SET);
		    break;
		case '/':
		    write(STDOUT_FILENO, "/", 1);
		    if (inputstr(searchstr, sizeof searchstr - 1) &&
			    *searchstr != '\0')
		    {
			do_delay = 0;
			do_search = 1;
			do_output = 0;
			had_to_output = 0;
		    }
		    write(STDOUT_FILENO, "\010 \010", 3);
		    break;
		case 'g':
		    o = 0;
		    fseeko(input, 0, SEEK_SET);
		    do_search = 0;
		    do_delay = do_output = 1;
		    had_to_output = 0;
		    break;
		case 'm':
		    read(STDOUT_FILENO, &c, 1);
		    if (c >= 'a' && c <= 'z')
			marknum = c - 'a';
		    else if (c >= 'A' && c <= 'Z')
			marknum = c - 'A';
		    else
		    {
			write(STDOUT_FILENO, "\a", 1);
			break;
		    }
		    marks[marknum] = o - 12 - r;
		    if (marks[marknum] < 0)
			marks[marknum] = 0;
		    break;
		case '\'': case '`':
		    read(STDOUT_FILENO, &c, 1);
		    if (c >= 'a' && c <= 'z')
			marknum = c - 'a';
		    else if (c >= 'A' && c <= 'Z')
			marknum = c - 'A';
		    else
		    {
			write(STDOUT_FILENO, "\a", 1);
			break;
		    }
		    o = marks[marknum];
		    cscur = cscount - 1;
		    while (o <= clrscroffsets[cscur] && cscur > 0)
			cscur--;
		    o = clrscroffsets[cscur];
		    fseeko(input, o, SEEK_SET);
		    quick_until = marks[marknum];
		    do_delay = do_search = 0;
		    do_output = 1;
		    break;
		default:
		    write(STDOUT_FILENO, "\a", 1);
	    }
	}
	if (had_to_output)
	    if (write(STDOUT_FILENO, data, r) != r)
		cleanup_term("@write stdout: ");
    }

    if (errno != 0)
	cleanup_term("@read input: ");
    else
        cleanup_term(NULL);

    return 0;
}

/* vim:ts=8:cin:sw=4
 *  */

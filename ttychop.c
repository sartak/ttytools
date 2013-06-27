/*
 * Copyright (c) 2006 Sean Kelly
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. 
 */
/*
 * Program designed for chopping ttyrecs.
 * version r2
 */
/*
 * Changelog
 *
 * r2:
 * added stdlib.h header
 *
 * r1:
 * initial release
 */
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* prints usage statement */
void usage()
{
  printf("Usage:\n"
      "ttychop [-c] [-s start] [-e end] INFILE OUTFILE\n"
      "INFILE must be either a file or - for stdin."
      " A - for OUTFILE means stdout\n"
      "-c  Count number of frames. Overrides previous switches.\n"
      "-d  Specify if ttyrec uses DEC graphics.\n"
      "-s  Start frame or missing to denote beginning. 0 is the first frame.\n"
      "-e  End frame or missing to denote end. -1 is the last frame.\n"
    );
  exit(0);
}

char inbuf[10240];
char outbuf[11264];
int outfd = -1, infd = 0;

/* Fills input buffer. Returns bytes read. */
int fillbuffer(int offset)
{
  int n, len = sizeof(inbuf) - offset;
  if (len <= 0)
    return -1;
  if ((n = read(infd, inbuf+offset, len)) != -1)
    return n;
  return -1;
}

void flushbuffer(int len)
{
  if (write(outfd, outbuf, len) == -1)
  {
    fprintf(stderr, "Unable to write to output file: '%s'\n", strerror(errno));
    exit(3);
  }
}

#define MAKE_LE(x) \
  (((x&0x000000FFU) << 24) | \
   ((x&0x0000FF00U) <<  8) | \
   ((x&0x00FF0000U) >>  8) | \
   ((x&0xFF000000U) >> 24))

int is_LE()
{
  static int ret = -1;

  if (ret == -1)
  {
    uint32_t moo = 0x12345678;
    uint8_t *x   = (uint8_t *) &moo;
    /* LE: 0x78, 0x56, 0x34, 0x12
     * BE: 0x12, 0x34, 0x56, 0x78
     * ME: don't care
     */
    if (*x == 0x78)
      ret = 1;
    else
      ret = 0;
  }
  
  return ret;
}

uint32_t to_LE(uint32_t i)
{
  if (is_LE())
    return i;
  else
    return MAKE_LE(i);
} 

/* Get frame length, ignore other two */
void ttyheader(const char *buf, int32_t *flen)
{
  int32_t *t;
  t = (int32_t *)&buf[8];
  *flen = to_LE(*t);
}

/* Counts number of frames. */
int count(int frames, int out)
{
  unsigned int count = 0;
  int n, pos, offset;
  int32_t flen = 0;
  /*
   * continue is used for each frame
   * break is used when needing to refill the buffer
   * I did this mostly for personal amusement.
   */
  pos = 0;
  offset = 0;
  while ((n = fillbuffer(offset)) > 0)
  {
    n += offset;
    do
    {
      offset = 0;
      if (flen == 0)
      {
        if (n - pos >= 12)
        {
          ttyheader(inbuf+pos, &flen);
          flen += 12;
        }
        else
        {
          /* ends in middle of flen header*/
          offset = n - pos;
          memmove(inbuf, inbuf+pos, offset);
          break;
        }
      }
      if (n - pos >= flen)
      {
        if (out)
          memcpy(outbuf+pos, inbuf+pos, flen);
        pos += flen;
        flen = 0;
        count++;
        if (frames != 0 && count == frames)
        {
          lseek(infd, pos - n, SEEK_CUR);
          goto end;
        }
        continue;
      }
      else
      {
        if (out)
          memcpy(outbuf+pos, inbuf+pos, n - pos);
        flen -= (n - pos);
        pos = n;
        break;
      }
    } while (1);
    if (out)
      flushbuffer(pos);
    pos = 0;
  }
end:
  if (out)
    flushbuffer(pos);
  return count;
}
void chop(int start, int end)
{
  int len = end - start;
  if (end != -1 && len <= 0)
  {
    printf("End smaller than start, exiting..\n");
    exit(0);
  }
  if (start > 0)
    count(start, 0);
  if (end == -1)
    len = 0;
  count(len, 1);
}

int main(int argc, char **argv)
{
  int ch;
  int start = 0, end = -1;
  int cnt = 0;
  if (argc == 1)
    usage();
  while ((ch = getopt(argc, argv, "ce:hs:")) != -1)
  {
    switch (ch)
    {
      case 'c':
        cnt = 1;
        break;
      case 'e':
        end = atoi(optarg);
        if (end < 0)
          end = -1;
        break;
      case 's':
        start = atoi(optarg);
        if (start < 0)
          start = 0;
        break;
      case 'h':
        /* FALLTHROUGH */
      default:
        usage();
        /* NOTREACHED */
    }
  }
  argc -= optind;
  argv += optind;
  if (argc > 0)
  {
    /* INFILE */
    if (!strcmp(argv[0], "-"))
      infd = 0;
    else
    {
      if ((infd = open(argv[0], O_RDONLY)) == -1)
      {
        fprintf(stderr, "Could not open '%s' for reading: %s\n", argv[0], strerror(errno));
        exit(1);
      }
    }
    if (cnt == 1)
      goto count;
  }
  if (argc > 1)
  {
    /* OUTFILE */
    if (!strcmp(argv[1], "-"))
      outfd = 1;
    else
    {
      if ((outfd = open(argv[1], O_WRONLY|O_CREAT|O_TRUNC,
       S_IRUSR|S_IWUSR)) == -1)
      {
        fprintf(stderr, "Could not open '%s' for writing: %s\n", argv[1], strerror(errno));
        exit(1);
      }
    }
  }
  else
  {
    fprintf(stderr, "Did not specify INFILE and/or OUTFILE\n");
    exit(2);
  }
  if (cnt)
  {
count:
    printf("Frames: %d\n", count(0, 0));
    exit(0);
  }
  else
    chop(start, end);
  return 0;
}

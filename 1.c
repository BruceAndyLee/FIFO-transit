#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <string.h>

#define DECIMAL 10
#define BUFFER_SZ 5
#define MAX_FIFO_NAME_LEN 10

typedef int fid;

void reader_main();
void writer_main(const char*);
const char* itoa(long int);
void reverse(char*);
void interact(fid);
void convey(const char*, fid);
int  make_blocking(fid);

int main(int argc, char* argv[])
{
	errno = 0;
	if (argc < 2)
	{
		printf("Usage: %s [process type]\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	if (!strcmp(argv[1], "reader"))
		reader_main();
	if (!strcmp(argv[1], "writer") && argc < 3)
	{
		printf("Usage: %s %s [file to pipe]\n", argv[0], argv[1]);
		exit(EXIT_FAILURE);
	}
	else if (!strcmp(argv[1], "writer"))
		writer_main(argv[2]);

	exit(EXIT_SUCCESS);
}

int make_blocking(fid fd)
{
	int flgs = fcntl(fd, F_GETFL);
	if (flgs == -1) return 0;

	return fcntl(fd, F_SETFL, F_SETFL, flgs & ~O_NONBLOCK) != -1;
}


const char* itoa(long int n)
{	
	if (n < 0)
		return NULL;

	int len = 1, a = n;
	for (; (a /= 10) > 0; len++);

	char* s = (char*) calloc (len, sizeof(char));

	int i = 0;
	do 
	{   
		s[i++] = n % 10 + '0';   
	} while ((n /= 10) > 0);

	s[i] = '\0';
	reverse(s);
	return s;
}

void reverse(char* s)
{
	int i = 0, j = strlen(s) - 1;
	char c;

	for (; i < j; i++, j--) 
	{
		c		= s[i];
		s[i]	= s[j];
		s[j]	= c;
	}
}

void reader_main()
{
	/* creating my_indiv_fifo */
	const char* fifo_name = itoa(getpid());
	if (!fifo_name)
	{
		printf("reader: error: cannot create individual fifoname(%d)\n", __LINE__);
		exit(EXIT_FAILURE);
	}
	if (mkfifo(fifo_name, 0666 | O_CREAT) == -1 && errno != EEXIST)
	{
		printf("reader: fifo open: error: %s(%d)\n", strerror(errno), __LINE__);
		exit(EXIT_FAILURE);
	}

	/* creating + opening sync fifo */
	if (mkfifo("syncf", 0666 | O_CREAT) == -1 && errno != EEXIST)
	{
		printf("reader: mk(sync)fifo: error: %s(%d)\n", strerror(errno), __LINE__);
		exit(EXIT_FAILURE);
	}	
	fid sync_fifo = open("syncf", O_WRONLY);
	if (sync_fifo == -1)
	{
		printf("reader: open(syncf): error: %s(%d)\n", strerror(errno), __LINE__);
		exit(EXIT_FAILURE);
	}

	printf ("reader: will open fifo file %s\n", fifo_name);
	/* opening my fifo */
	fid fifo = open(fifo_name, O_RDONLY | O_NONBLOCK);
	if (fifo == -1)
	{
		printf("reader: open(fifo): error: %s(%d)\n", strerror(errno), __LINE__);
		exit(EXIT_FAILURE);
	}
	int fifo_num = getpid();
	/* conveying my fifo name to writer */
	if (write(sync_fifo, &fifo_num, sizeof(int)) == -1)
	{
		printf("reader: write: error: %s(%d)\n", strerror(errno), __LINE__);
		exit(EXIT_FAILURE);
	}
	usleep(1000);
	interact(fifo);
	
	char* unlink_str = (char*) calloc (10, sizeof(char));
	sprintf(unlink_str, "rm %s", fifo_name);
	system(unlink_str);
	free(unlink_str);
}

void writer_main(const char* fname)
{
	/* making + opening sync fifo */
	if (mkfifo("syncf", 0666 | O_CREAT) == -1 && errno != EEXIST)
	{
		printf("writer: mk(sync)fifo: error: %s(%d)\n", strerror(errno), __LINE__);
		exit(EXIT_FAILURE);
	}
	fid sync_fifo = open("syncf", O_RDONLY);
	if (sync_fifo == -1)
	{
		printf("writer: open: error: %s(%d)\n", strerror(errno), __LINE__);
		exit(EXIT_FAILURE);
	}

	/* receiving indiv fifo name */
	pid_t fifo_num = 0;
	char* fifo_name = (char*) calloc (MAX_FIFO_NAME_LEN, sizeof(char));
	if (read(sync_fifo, &fifo_num, sizeof(int)) == -1)
	{
		printf("Writer: read: error: %s(%d)\n", strerror(errno), __LINE__);
		exit(EXIT_FAILURE);
	}
	sprintf(fifo_name, "%d", fifo_num);
	printf("my fifo name: %s\n", fifo_name);

	/* opening indiv fifo */
	fid fifo = open(fifo_name, O_WRONLY | O_NONBLOCK); //will fail if reader didn't manage to open the other end
	if (fifo == -1)
	{
		printf("writer: open(fifo): error: %s(%d)\n", strerror(errno), __LINE__);
		exit(EXIT_FAILURE);
	}
	convey(fname, fifo);
}

void interact(fid fifo) 
{
	if (!make_blocking(fifo))
	{
		printf("reader: fcntl: error: %s(%d)\n", strerror(errno), __LINE__);
		exit(EXIT_FAILURE);
	}

	printf("reader: gonna print\n");
	char* buffer = (char*) calloc (BUFFER_SZ, sizeof(char));

	int rd = 0;
	while ((rd = read(fifo, buffer, BUFFER_SZ)) > 0)
	{
		printf("%s", buffer);
		usleep(5000);
	}
	if (rd == -1)
	{
		printf("reader: interact: error: %s(%d)\n", strerror(errno), __LINE__);
	}
	printf("%s", buffer);
}

void convey(const char* fname, fid fifo)
{
	if (!make_blocking(fifo))
	{
		printf("writer: fcntl: error: %s(%d)\n", strerror(errno), __LINE__);
		exit(EXIT_FAILURE);
	}
	
	printf("file name: %s\n", fname);
	fid file = open(fname, O_RDONLY);
	if (file == -1)
	{
		printf("writer: convey: error: %s(%d)\n", strerror(errno), __LINE__);
		exit(EXIT_FAILURE);
	}
	char* buffer = (char*) calloc (BUFFER_SZ, sizeof(char));
	int rd = 0;
	while ((rd = read(file, buffer, BUFFER_SZ)) != 0)
	{
		if (write(fifo, buffer, rd) == -1)
		{
			printf("writer: write: error: %s(%d)", strerror(errno), __LINE__);
			exit(EXIT_FAILURE);
		}
	}
	write(fifo, buffer, rd);
	close(file);
}

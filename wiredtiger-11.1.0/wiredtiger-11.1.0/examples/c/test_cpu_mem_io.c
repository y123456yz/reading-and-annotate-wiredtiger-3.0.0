/*#include <time.h>
#include<stdio.h>
#include<string.h>
int main () {
  time_t T1 = time(NULL);
  unsigned long long limit = 1000000ULL * 5000;
  // volatile used to ensure that loop is not optimized away
  long long lresult = 0; 

  unsigned long long x = 100;
  unsigned long long i;
  for ( i =0; i< limit; i++) {
      x*=13;
  }
  time_t T2 = time(NULL);
  double time_diff = difftime(T2, T1);
  printf("yang tst.........:%f\r\n", time_diff);
}

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
int main () {
  time_t T1 = time(NULL);
  long long i;
  for ( i = 0;i< 990000000LL; i++)
  {
     char *p =malloc(500);
     memset(p, 0, 500);
     free(p);
  }
  time_t T2 = time(NULL);
  double time_diff = difftime(T2,T1);
  printf("yang tst.........:%f\r\n", time_diff);
}
*/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

int main() {
    int fd;  
    
    #define maxbuf  4096
    
    char buffer[maxbuf] = "hewhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldorldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello worldhello world\r\n";
    //char buffer[maxbuf] = "1111111111";
    char buffer2[maxbuf];
    int str_len = strlen(buffer);
    srand(time(NULL));
    int random_number = rand();
    time_t T1 = time(NULL);
    
    if ((fd = open("test_io.txt", O_RDWR | O_CREAT, 0666)) == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }
    ftruncate(fd,100000000000);

    int i;
    for (i = 0; i < 100000; i++) {
        random_number = rand();
        random_number = random_number % 24414062;

        //snprintf(buffer2, maxbuf, "%s_%d_%d\r\n", buffer, i, random_number);
        //printf("yang test......random_number...:%d\r\n", random_number);24,414,062
        pwrite(fd, buffer2, maxbuf, random_number * maxbuf);
    }

    time_t T2 = time(NULL);
    double time_diff = difftime(T2, T1);
    printf("yang tst.........:%f\r\n", time_diff);
/*
    write(fd, buffer, str_len);
   // read(fd, buffer2, maxbuf);
    pread(fd, buffer2, maxbuf, 0);
    printf("read1:%s\n", buffer2);

    //lseek(fd, SEEK_SET, strlen(buffer));
    //write(fd, buffer, sizeof(buffer));
    pwrite(fd, buffer, str_len, str_len);

    pread(fd, buffer2, maxbuf, 10);
    //read(fd, buffer2, maxbuf);
    printf("read2:%s\n", buffer2);
*/
    close(fd);
    return EXIT_SUCCESS;
}



#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <aio.h>

#define TNAME "aio_wirte/1-1.c"

int main() {
    char tmpfname[256];
    #define BUF_SIZE 512
    char buf[BUF_SIZE];
    char check[BUF_SIZE+1];
    int fd;
    struct aiocb aiocb;
    int err;
    int ret;

    snprintf(tmpfname, sizeof(tmpfname), "pts_aio_write_1_1_%d", getpid());
    unlink(tmpfname);

    fd = open(tmpfname, O_CREAT|O_RDWR|O_EXCL, S_IRUSR|S_IWUSR);

    if (fd == -1)
    {
        printf(TNAME " Error at open(): %s\n", strerror(errno));
        exit(1);
    }

    unlink(tmpfname);

    memset(buf, 0xaa, BUF_SIZE);
    memset(&aiocb, 0, sizeof(struct aiocb));

    /* aiocb 구조체 초기화 */
    aiocb.aio_fildes = fd;
    aiocb.aio_buf    = buf;
    aiocb.aio_nbytes = BUF_SIZE;

    /* 쓰기 요청을 저장하는 aiocb를 큐에 추가 */
    if (aio_write(&aiocb) == -1)
    {
        printf(TNAME " Error at aio_write(): %s\n", strerror(errno));
        close(fd);
        exit(2);
    }

    /* 트랜잭션 종료까지 대기 */
    while (aio_error(&aiocb) == EINPROGRESS);

    /* 종료된 트랜잭션에 대한 오류값과 반환 길이 저장 */
    err = aio_error(&aiocb);
    ret = aio_return(&aiocb);

    /* 오류값을 통한 예외 처리 */
    if (err != 0)
    {
        printf(TNAME " Error at aio_error(): %s\n", strerror(err));
        close(fd);
        exit(2);
    }

    /* 반환 길이를 통한 예외 처리 */
    if (ret != BUF_SIZE)
    {
        printf(TNAME " Error at aio_return()\n");
        close(fd);
        exit(2);
    }

    /* 쓰여진 값들을 검사한다. */
    if (lseek(fd, 0, SEEK_SET) == -1)
    {
        printf(TNAME " Error at lseek(): %s\n",
                strerror(errno));
        close(fd);
        exit(2);
    }

    /* 썼던 크기보다 더 많이 읽기를 시도함으로써 쓰여진 크기를 확인한다. */
    check[BUF_SIZE] = 1;

    if (read(fd, check, BUF_SIZE + 1) != BUF_SIZE)
    {
        printf(TNAME " Error at read(): %s\n",
                strerror(errno));
        close(fd);
        exit(2);
    }

    if (check[BUF_SIZE] != 1)
    {
        printf(TNAME " Buffer overflow\n");
        close(fd);
        exit(2);
    }

    if (memcmp(buf, check, BUF_SIZE))
    {
        printf(TNAME " Bad value in buffer\n");
        close(fd);
        exit(2);
    }

    close(fd);
    printf("Test PASSED\n");
    return 0;
}


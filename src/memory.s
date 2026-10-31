; Keep room for one page-aligned 1KB ProDOS I/O buffer plus allocator slack.
; Without this assertion a growing binary can boot but fail every fopen().
.import __BSS_RUN__, __BSS_SIZE__, __STACKSIZE__
.assert __BSS_RUN__ + __BSS_SIZE__ + $0600 <= $BF00 - __STACKSIZE__, lderror, "Need >=1536 bytes of heap for aligned ProDOS I/O buffer"

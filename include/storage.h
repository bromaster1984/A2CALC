#ifndef A2CALC_STORAGE_H
#define A2CALC_STORAGE_H

/* Zero is success. Files use ASCII and ProDOS-compatible uppercase names. */
#define STORAGE_OK 0
#define STORAGE_IO 1
#define STORAGE_FORMAT 2
#define STORAGE_BUSY 3

unsigned char storage_save(const char *path);
unsigned char storage_load(const char *path);
unsigned char storage_xlsx(const char *path);

#endif

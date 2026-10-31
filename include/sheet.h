#ifndef A2CALC_SHEET_H
#define A2CALC_SHEET_H
#include <stdint.h>
#define SHEET_COLS 26
#define SHEET_ROWS 100
#define SHEET_MAX_CELLS 128
#define SHEET_TEXT_MAX 63
#define SHEET_SCALE 100L
#define SHEET_EMPTY 0
#define SHEET_NUMBER 1
#define SHEET_TEXT 2
#define SHEET_FORMULA 3
#define SHEET_OK 0
#define SHEET_ERR_REF 1
#define SHEET_ERR_VALUE 2
#define SHEET_ERR_DIV0 3
#define SHEET_ERR_NUM 4
#define SHEET_ERR_NAME 5
#define SHEET_ERR_CYCLE 6
#define SHEET_ERR_FULL 7
#define SHEET_ERR_LONG 8
#define SHEET_ERR_DEPTH 9
#define SHEET_ERR_PARSE 10
typedef int32_t SheetNum;
void sheet_reset(void);
unsigned char sheet_set(unsigned char col,unsigned char row,const char *raw);
const char *sheet_get(unsigned char col,unsigned char row);
void sheet_recalc(void);
SheetNum sheet_value(unsigned char col,unsigned char row);
unsigned char sheet_error(unsigned char col,unsigned char row);
unsigned char sheet_kind(unsigned char col,unsigned char row);
const char *sheet_error_text(unsigned char error);
void sheet_format(SheetNum value,char *out);
unsigned char sheet_count(void);
const char *sheet_at(unsigned char index,unsigned char *col,unsigned char *row);
unsigned char sheet_paste(const char *raw,unsigned char sc,unsigned char sr,unsigned char dc,unsigned char dr);
unsigned char sheet_copy(unsigned char sc,unsigned char sr,unsigned char dc,unsigned char dr);
#endif

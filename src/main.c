/* A2CALC - Apple IIc/ProDOS, 80-column console. */
#include <conio.h>
#include <apple2enh.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "sheet.h"
#include "storage.h"
#include "a2mouse.h"

#define VIEW_ROWS 18
static unsigned char col, row, left, top, width = 12, dirty;
static unsigned char copied, copy_col, copy_row, undo_valid, undo_col, undo_row;
static char undo_text[SHEET_TEXT_MAX+1], copied_text[SHEET_TEXT_MAX+1];
static char edit[SHEET_TEXT_MAX+1], path[80] = "/DATA/SHEET.A2C";
static char message[80] = "Ctrl-L: help. Ctrl-D: demo. Save files on drive 2 volume DATA.";
static A2Mouse mouse_state;

static unsigned char columns(void) { return 75 / width; }
static void line(unsigned char y, const char *s) {
    unsigned char i;
    gotoxy(0,y);
    for (i=0; i<79; ++i) cputc(s && *s ? *s++ : ' ');
}
static void status(const char *s) {
    strncpy(message,s,79); message[79]=0;
    a2mouse_hide(); line(22,message); a2mouse_show();
}
static void remember(void) {
    undo_col=col; undo_row=row;
    strcpy(undo_text,sheet_get(col,row)); undo_valid=1;
}
static void draw_cell(unsigned char c, unsigned char r) {
    unsigned char i, n, selected;
    char number[18]; const char *s;
    if(c<left || c>=left+columns() || r<top || r>=top+VIEW_ROWS) return;
    selected=(c==col && r==row); revers(selected);
    gotoxy(4+(c-left)*width,3+r-top);
    if(sheet_error(c,r)) s=sheet_error_text(sheet_error(c,r));
    else if(sheet_kind(c,r)==1 || sheet_kind(c,r)==3) {
        sheet_format(sheet_value(c,r),number); s=number;
    } else { s=sheet_get(c,r); if(*s=='\'') ++s; }
    n=strlen(s);
    if(n>width-1 && (sheet_kind(c,r)==1 || sheet_kind(c,r)==3)) {
        for(i=0; i<width-1; ++i) cputc('#');
    } else if(sheet_kind(c,r)==1 || sheet_kind(c,r)==3) {
        for(i=n; i<width-1; ++i) cputc(' ');
        for(i=0; i<n; ++i) cputc(s[i]);
    } else {
        if(n>width-1) n=width-1;
        for(i=0; i<n; ++i) cputc(s[i]);
        for(i=n; i<width-1; ++i) cputc(' ');
    }
    revers(0); cputc('|');
}
static void details(void) {
    char b[80];
    sprintf(b,"%c%u %s %u/%u cells%s",'A'+col,(unsigned)row+1,
        sheet_kind(col,row)==3 ? "formula" : "",(unsigned)sheet_count(),
        (unsigned)SHEET_MAX_CELLS,dirty ? "  UNSAVED" : "");
    line(1,b); line(21,sheet_get(col,row)); line(22,message);
}
static void draw(void) {
    unsigned char x,y; char b[8];
    a2mouse_hide(); clrscr();
    revers(1); line(0," A2CALC | New | Open | Save | XLSX | Goto | Find | Help"); revers(0);
    for(x=0; x<columns() && left+x<SHEET_COLS; ++x) {
        gotoxy(4+x*width,2); revers(1);
        cputc('A'+left+x); for(y=1;y<width;++y) cputc(' '); revers(0);
    }
    for(y=0;y<VIEW_ROWS && top+y<SHEET_ROWS;++y) {
        sprintf(b,"%3u ",(unsigned)top+y+1); cputsxy(0,3+y,b);
        for(x=0;x<columns() && left+x<SHEET_COLS;++x) draw_cell(left+x,top+y);
    }
    details(); line(23,"Arrows:move Enter:edit  ^S:save ^O:open ^X:xlsx ^C:copy ^V:paste ^L:help");
    a2mouse_show();
}
static void move_to(unsigned char c,unsigned char r) {
    unsigned char oc=col,orr=row,ol=left,ot=top;
    col=c; row=r;
    if(col<left) left=col;
    if(col>=left+columns()) left=col-columns()+1;
    if(row<top) top=row;
    if(row>=top+VIEW_ROWS) top=row-VIEW_ROWS+1;
    if(ol!=left || ot!=top) draw();
    else { a2mouse_hide(); draw_cell(oc,orr); draw_cell(col,row); details(); a2mouse_show(); }
}
/* Inline editor; Escape leaves the cell/file untouched. */
static unsigned char prompt(const char *label,char *buffer,unsigned char max) {
    unsigned char len,pos,key,i,start;
    len=strlen(buffer); if(len>max) len=max; buffer[len]=0; pos=len;
    a2mouse_hide(); cursor(1); line(22,label);
    while(1) {
        start=pos>75 ? pos-75 : 0;
        gotoxy(0,23);
        for(i=0;i<79;++i) cputc(start+i<len ? buffer[start+i] : ' ');
        gotoxy(pos-start,23); key=cgetc();
        if(key==CH_ESC) { cursor(0); a2mouse_show(); return 0; }
        if(key==CH_ENTER) { cursor(0); a2mouse_show(); return 1; }
        if(key==CH_CURS_LEFT) { if(pos) --pos; }
        else if(key==CH_CURS_RIGHT) { if(pos<len) ++pos; }
        else if(key==CH_DEL) {
            if(pos) { memmove(buffer+pos-1,buffer+pos,len-pos+1); --pos; --len; }
        } else if(key==1) pos=0;
        else if(key==5) pos=len;
        else if(key==11) { buffer[0]=0;len=pos=0; }
        else if(key>=32 && key<127 && len<max) {
            memmove(buffer+pos+1,buffer+pos,len-pos+1); buffer[pos++]=key; ++len;
        }
    }
}
static unsigned char confirm(const char *s) {
    unsigned char k;
    a2mouse_hide(); line(22,s); line(23,"Y = yes. Any other key = cancel.");
    k=cgetc(); a2mouse_show(); return k=='y'||k=='Y';
}
static void edit_cell(unsigned char first) {
    unsigned char e;
    strcpy(edit,sheet_get(col,row));
    if(first) { edit[0]=first; edit[1]=0; }
    if(prompt("Cell: Enter=accept Esc=cancel Delete=erase ^K=clear ^A/^E=home/end",edit,SHEET_TEXT_MAX)) {
        remember(); e=sheet_set(col,row,edit);
        if(e) status(sheet_error_text(e));
        else { dirty=1; sheet_recalc(); strcpy(message,"Cell updated. Ctrl-Z undoes the last cell edit."); }
    }
    draw();
}
static void help(void) {
    a2mouse_hide(); clrscr();
    cputs("A2CALC 1.0 - Apple IIc, 80 columns, ProDOS\r\n\r\n"
      "Arrows move; Tab next column; Enter edits existing cell.\r\n"
      "Type to replace cell; Delete clears; Esc cancels an edit.\r\n"
      "Click a cell to select; click selected cell to edit.\r\n\r\n"
      "^N New  ^O Open  ^S Save  ^X Export XLSX  ^Q Quit\r\n"
      "^G Go to A1  ^F Find text  ^W Column width  ^D Demo\r\n"
      "^C Copy  ^V Paste (relative refs)  ^Z Undo last cell edit\r\n\r\n"
      "Formulas start = ; use English names, decimal dot, commas.\r\n"
      "SUM AVERAGE MIN MAX COUNT IF ABS ROUND INT MOD AND OR NOT SQRT POWER\r\n"
      "Example: =SUM(A1:B5)   =IF(A1>10,A1*2,0)\r\n"
      "References A1, $A$1; ranges; + - * / and comparisons.\r\n\r\n"
      "26 columns x 100 rows; 128 nonempty cells; 63 chars/cell.\r\n"
      "Numbers: 2 decimal places. No dates, charts or text formulas.\r\n"
      "Save native .A2C for reopening. XLSX is export-only here.\r\n"
      "Use tools/convert.py on a PC to import supported XLSX/CSV.\r\n"
      "Default data path /DATA/SHEET.A2C (second drive volume DATA).\r\n"
      "Program disk stays in drive 1. ASCII text only.\r\n\r\n"
      "Press any key to return.");
    cgetc(); draw();
}
static void demo(void) {
    sheet_reset();
    sheet_set(0,0,"A2CALC DEMO"); sheet_set(0,2,"Item"); sheet_set(1,2,"Quantity");
    sheet_set(2,2,"Price"); sheet_set(3,2,"Total");
    sheet_set(0,3,"Diskettes"); sheet_set(1,3,"10"); sheet_set(2,3,"2.50"); sheet_set(3,3,"=B4*C4");
    sheet_set(0,4,"Mouse"); sheet_set(1,4,"1"); sheet_set(2,4,"35"); sheet_set(3,4,"=B5*C5");
    sheet_set(0,6,"Grand total"); sheet_set(3,6,"=SUM(D4:D5)");
    sheet_set(0,8,"Average"); sheet_set(3,8,"=AVERAGE(D4:D5)");
    sheet_recalc(); dirty=1; undo_valid=0; col=row=left=top=0;
}
static void file_action(unsigned char action) {
    char p[80]; unsigned char result; FILE *f;
    strcpy(p,path);
    if(action=='X') { strcpy(p,"/DATA/SHEET.XLSX"); }
    if(action=='O' && dirty && !confirm("Discard unsaved changes and open another sheet?")) {draw();return;}
    if(!prompt(action=='X' ? "Export real XLSX: enter path (volume must be mounted)" :
        "Native A2C file: full path, e.g. /DATA/SHEET.A2C",p,79)) {draw();return;}
    if(!p[0]) {draw();return;}
    if(action!='O') {
        f=fopen(p,"rb");
        if(f) { fclose(f); if(!confirm("File exists. Replace it?")) {draw();return;} }
    }
    a2mouse_hide(); line(22,"Disk operation in progress. Do not remove disk.");
    if(action=='O') result=storage_load(p);
    else if(action=='X') result=storage_xlsx(p);
    else result=storage_save(p);
    a2mouse_show();
    if(result==STORAGE_BUSY) strcpy(message,"Scratch file A2CALC.TMP/BAK exists. Preserve/recover it on a PC first.");
    else if(result==STORAGE_FORMAT) strcpy(message,"Invalid or unsupported A2C file. Sheet was not loaded.");
    else if(result) strcpy(message,"Disk operation failed: check disk, path, free space and write protect.");
    else {
        if(action!='X') {strcpy(path,p); dirty=0;}
        if(action=='O') {col=row=left=top=0; undo_valid=0; sheet_recalc();}
        strcpy(message,action=='X' ? "XLSX exported. Keep native A2C too for reopening on Apple IIc." : "Disk operation completed.");
    }
    draw();
}
static void go_to(void) {
    unsigned int r; char *p;
    edit[0]=0;
    if(prompt("Go to cell (A1 to Z100):",edit,8)) {
        p=edit; if(*p=='$') ++p;
        if(toupper(*p)>='A' && toupper(*p)<='Z') {
            unsigned char c=toupper(*p++)-'A'; if(*p=='$') ++p;
            r=atoi(p); while(isdigit(*p)) ++p;
            if(r && r<=SHEET_ROWS && !*p) {move_to(c,r-1);draw();return;}
        }
        strcpy(message,"Invalid cell reference.");
    }
    draw();
}
static unsigned char contains(const char *s,const char *q) {
    unsigned char n;
    for(;*s;++s) {
        for(n=0;q[n] && s[n] && toupper(q[n])==toupper(s[n]);++n) {}
        if(!q[n]) return 1;
    }
    return 0;
}
static void find_text(void) {
    unsigned char i,c,r; const char *s;
    edit[0]=0;
    if(prompt("Find first matching cell (text or formula, case insensitive):",edit,63) && edit[0]) {
        for(i=0;i<sheet_count();++i) {
            s=sheet_at(i,&c,&r); if(contains(s,edit)) { move_to(c,r);draw();return; }
        }
        strcpy(message,"Text not found.");
    }
    draw();
}
static void command(unsigned char k) {
    unsigned char e;
    if(k==19) file_action('S');
    else if(k==15) file_action('O');
    else if(k==24) file_action('X');
    else if(k==12) help();
    else if(k==7) go_to();
    else if(k==6) find_text();
    else if(k==23) {width=width==12?18:width==18?25:12;left=col;draw();}
    else if(k==14 || k==4) {
        if(!dirty || confirm("Discard current sheet and unsaved changes?")) {
            if(k==4) demo();
            else {sheet_reset();col=row=left=top=dirty=undo_valid=0;}
        }
        draw();
    } else if(k==3) {copied=1;copy_col=col;copy_row=row;strcpy(copied_text,sheet_get(col,row));status("Cell copied. Ctrl-V pastes with relative formula references.");}
    else if(k==22 && copied) {
        remember(); e=sheet_paste(copied_text,copy_col,copy_row,col,row);
        if(e) status(sheet_error_text(e)); else {dirty=1;sheet_recalc();} draw();
    } else if(k==26 && undo_valid) {
        e=sheet_set(undo_col,undo_row,undo_text);
        if(!e) {dirty=1;undo_valid=0;sheet_recalc();} draw();
    }
}
int main(void) {
    unsigned char k;
    videomode(VIDEOMODE_80COL); cursor(0); sheet_reset();
    a2mouse_init(); draw();
    while(1) {
        a2mouse_poll(&mouse_state);
        if(mouse_state.clicked) {
            if(mouse_state.y>=3 && mouse_state.y<3+VIEW_ROWS && mouse_state.x>=4) {
                unsigned char c=left+(mouse_state.x-4)/width,r=top+mouse_state.y-3;
                if(c<SHEET_COLS && r<SHEET_ROWS && c<left+columns()) {
                    if(c==col && r==row) edit_cell(0); else move_to(c,r);
                }
            } else if(mouse_state.y==0) {
                k=mouse_state.x; if(k>=10 && k<15) command(14);
                else if(k>=15 && k<22) command(15);
                else if(k>=22 && k<29) command(19);
                else if(k>=29 && k<36) command(24);
                else if(k>=36 && k<43) command(7);
                else if(k>=43 && k<50) command(6);
                else if(k>=50) command(12);
            }
        }
        if(!kbhit()) continue;
        k=cgetc();
        if(k==CH_CURS_LEFT) {if(col)move_to(col-1,row);}
        else if(k==CH_CURS_RIGHT || k==9) {if(col<SHEET_COLS-1)move_to(col+1,row);}
        else if(k==CH_CURS_UP) {if(row)move_to(col,row-1);}
        else if(k==CH_CURS_DOWN) {if(row<SHEET_ROWS-1)move_to(col,row+1);}
        else if(k==CH_ENTER) edit_cell(0);
        else if(k==CH_DEL) {remember();sheet_set(col,row,"");dirty=1;sheet_recalc();draw();}
        else if(k==17) {if(!dirty || confirm("Quit and discard unsaved changes?")) break;draw();}
        else if(k<32) command(k);
        else if(k<127) edit_cell(k);
    }
    a2mouse_shutdown(); clrscr(); return 0;
}

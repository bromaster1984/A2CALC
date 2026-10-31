#include "sheet.h"
#include <string.h>
#include <ctype.h>

#define LIMIT 2147483647UL
#define MAX_DEPTH 16

typedef struct {
    char raw[SHEET_TEXT_MAX+1];
    SheetNum value;
    unsigned char col,row,kind,error,state;
} Cell;
typedef struct {
    const char *s;
    unsigned char error,active,depth;
} Parser;
#ifdef __APPLE2__
/* ProDOS leaves the $0800-$1FFF workspace available after launch.  The
   application linker reserves it for a separately initialized cell bank. */
#pragma bss-name(push,"LOWBSS")
static Cell lowcells[80];
#pragma bss-name(pop)
static Cell highcells[SHEET_MAX_CELLS-80];
static Cell *cell_pointer(unsigned char i)
{ return i<80 ? &lowcells[i] : &highcells[i-80]; }
#define CELL(i) (*cell_pointer(i))
#else
static Cell cells[SHEET_MAX_CELLS];
#define CELL(i) cells[i]
#endif
static unsigned char used;
static SheetNum expression(Parser *p);
static SheetNum evaluate(unsigned char index,unsigned char depth);

static unsigned char find_cell(unsigned char col,unsigned char row)
{
    unsigned char i;
    for(i=0;i<used;++i) if(CELL(i).col==col && CELL(i).row==row) return i;
    return SHEET_MAX_CELLS;
}
static void skip(Parser *p) { while(*p->s==' ' || *p->s=='\t') ++p->s; }
static void fail(Parser *p,unsigned char error)
{ if(!p->error && p->active) p->error=error; }
static uint32_t magnitude(SheetNum a)
{ return a<0 ? (uint32_t)(-(a+1))+1UL : (uint32_t)a; }
static SheetNum signed_result(Parser *p,uint32_t n,unsigned char negative)
{
    if(n>LIMIT) { fail(p,SHEET_ERR_NUM); return 0; }
    return negative ? -(SheetNum)n : (SheetNum)n;
}
static SheetNum add(Parser *p,SheetNum a,SheetNum b)
{
    if((b>0 && a>(SheetNum)LIMIT-b) || (b<0 && a<-(SheetNum)LIMIT-b)) {
        fail(p,SHEET_ERR_NUM);return 0;
    }
    return a+b;
}
/* Divide a 64-bit unsigned product by a 32-bit divisor, without long long.
   This is needed on cc65, whose largest integer is 32 bits. */
static uint32_t muldiv(Parser *p,uint32_t a,uint32_t b,uint32_t d)
{
    uint32_t hi,lo,ah,al,old,q,r;
    unsigned char i,carry,bit;
    if(!d) { fail(p,SHEET_ERR_DIV0);return 0; }
    hi=lo=ah=0;al=a;
    for(i=0;i<32;++i) {
        if(b&1UL) { old=lo;lo+=al;hi+=ah+(lo<old); }
        ah=(ah<<1)|(al>>31);al<<=1;b>>=1;
    }
    if(hi>=d) { fail(p,SHEET_ERR_NUM);return 0; }
    q=0;r=hi;
    for(i=0;i<32;++i) {
        bit=(unsigned char)(lo>>31);lo<<=1;
        carry=(unsigned char)(r>>31);r=(r<<1)|bit;q<<=1;
        if(carry || r>=d) { r-=d;q|=1UL; }
    }
    /* Round to nearest cent, with ties away from zero. */
    if(r>=d-r) { if(q==0xFFFFFFFFUL) { fail(p,SHEET_ERR_NUM);return 0; } ++q; }
    if(q>LIMIT) { fail(p,SHEET_ERR_NUM);return 0; }
    return q;
}
static SheetNum multiply(Parser *p,SheetNum a,SheetNum b)
{ return signed_result(p,muldiv(p,magnitude(a),magnitude(b),100UL),(a<0)!=(b<0)); }
static SheetNum divide(Parser *p,SheetNum a,SheetNum b)
{ return signed_result(p,muldiv(p,magnitude(a),100UL,magnitude(b)),(a<0)!=(b<0)); }

static SheetNum number(Parser *p)
{
    uint32_t whole,fraction;
    unsigned char digits,saw,roundup;
    whole=fraction=0;digits=saw=roundup=0;
    while(isdigit((unsigned char)*p->s)) {
        saw=1;
        if(whole>21474836UL || (whole==21474836UL && *p->s>'4')) fail(p,SHEET_ERR_NUM);
        if(whole<21474837UL) whole=whole*10UL+(unsigned char)(*p->s-'0');
        ++p->s;
    }
    if(*p->s=='.') {
        ++p->s;
        while(isdigit((unsigned char)*p->s)) {
            saw=1;
            if(digits<2) fraction=fraction*10UL+(unsigned char)(*p->s-'0');
            else if(digits==2) roundup=*p->s>='5';
            if(digits<3) ++digits;
            ++p->s;
        }
    }
    if(!saw) { fail(p,SHEET_ERR_PARSE);return 0; }
    if(digits==1) fraction*=10;
    if(whole>21474836UL) { fail(p,SHEET_ERR_NUM);return 0; }
    return signed_result(p,whole*100UL+fraction+roundup,0);
}

/* A1 references include optional $ anchors, retained in raw formula text. */
static unsigned char reference(Parser *p,unsigned char *col,unsigned char *row)
{
    const char *start;
    unsigned int c,r;
    unsigned char saw;
    start=p->s; c=r=saw=0;
    if(*p->s=='$') ++p->s;
    while(isalpha((unsigned char)*p->s)) {
        c=c*26+(unsigned char)(toupper((unsigned char)*p->s)-'A'+1);
        if(c>256) c=256;
        ++p->s;
    }
    if(*p->s=='$') ++p->s;
    while(isdigit((unsigned char)*p->s)) {
        saw=1;r=r*10+(unsigned char)(*p->s-'0');
        if(r>1000) r=1000;
        ++p->s;
    }
    if(!c || !saw) { p->s=start;return 0; }
    if(c>SHEET_COLS || r<1 || r>SHEET_ROWS) { fail(p,SHEET_ERR_REF);*col=*row=0;return 1; }
    *col=(unsigned char)(c-1);*row=(unsigned char)(r-1);return 1;
}
static SheetNum ref_value(Parser *p,unsigned char col,unsigned char row,unsigned char text_error)
{
    unsigned char i;
    SheetNum n;
    if(!p->active) return 0;
    i=find_cell(col,row);
    if(i==SHEET_MAX_CELLS) return 0;
    if(CELL(i).kind==SHEET_TEXT) { if(text_error) fail(p,SHEET_ERR_VALUE);return 0; }
    n=evaluate(i,(unsigned char)(p->depth+1));
    fail(p,CELL(i).error);return n;
}

#define FN_SUM 1
#define FN_AVERAGE 2
#define FN_MIN 3
#define FN_MAX 4
#define FN_COUNT 5
#define FN_IF 6
#define FN_ABS 7
#define FN_ROUND 8
#define FN_INT 9
#define FN_MOD 10
#define FN_AND 11
#define FN_OR 12
#define FN_NOT 13
#define FN_SQRT 14
#define FN_POWER 15
static unsigned char function_id(const char *name)
{
    static const char * const names[]={"SUM","AVERAGE","MIN","MAX","COUNT","IF","ABS","ROUND","INT","MOD","AND","OR","NOT","SQRT","POWER"};
    unsigned char i;
    for(i=0;i<15;++i) if(!strcmp(name,names[i])) return i+1;
    return 0;
}
static SheetNum power(Parser *p,SheetNum base,SheetNum exponent)
{
    SheetNum result;
    int e;
    unsigned char negative;
    if(exponent%100 || exponent>3100 || exponent< -3100) { fail(p,SHEET_ERR_NUM);return 0; }
    e=(int)(exponent/100);negative=e<0;if(negative) e=-e;
    result=100;
    while(e) { if(e&1) result=multiply(p,result,base);e>>=1;if(e) base=multiply(p,base,base); }
    return negative ? divide(p,100,result) : result;
}
static SheetNum round_number(Parser *p,SheetNum value,SheetNum digits)
{
    uint32_t unit,a,n;
    int d;
    if(digits%100 || digits>200 || digits< -700) { fail(p,SHEET_ERR_NUM);return 0; }
    d=(int)(digits/100);unit=1;
    while(d<2) { unit*=10;++d; }
    a=magnitude(value);n=a/unit;
    if(a%unit>=unit-a%unit) ++n;
    if(n>LIMIT/unit) { fail(p,SHEET_ERR_NUM);return 0; }
    return signed_result(p,n*unit,value<0);
}
static SheetNum square_root(Parser *p,SheetNum n)
{
    uint32_t lo,hi,mid,best,v;
    if(n<0) { fail(p,SHEET_ERR_NUM);return 0; }
    lo=0;hi=463410;best=0;
    /* sqrt(n/100)*100 = sqrt(n*100); compare without overflowing. */
    while(lo<=hi) {
        mid=lo+(hi-lo)/2;
        if(!mid) { best=0;lo=1;continue; }
        v=(uint32_t)n/mid;
        if(mid/100<v || (mid/100==v && mid%100<=(((uint32_t)n%mid)*100)/mid)) { best=mid;lo=mid+1; }
        else hi=mid-1;
    }
    return (SheetNum)best;
}

static void accumulate(Parser *p,unsigned char fn,SheetNum value,SheetNum *result,unsigned int *count)
{
    if(!*count) {
        if(fn==FN_MIN || fn==FN_MAX) *result=value;
        if(fn==FN_AND) *result=100;
    }
    if(fn==FN_SUM || fn==FN_AVERAGE) *result=add(p,*result,value);
    else if(fn==FN_MIN && value<*result) *result=value;
    else if(fn==FN_MAX && value>*result) *result=value;
    else if(fn==FN_AND && !value) *result=0;
    else if(fn==FN_OR && value) *result=100;
    ++*count;
}
static SheetNum function(Parser *p,const char *name)
{
    unsigned char fn,c1,r1,c2,r2,c,r,i,oldactive,branch;
    unsigned int count,args;
    SheetNum result,v,a,b;
    Parser probe;
    fn=function_id(name);result=a=b=0;count=args=0;
    if(!fn) fail(p,SHEET_ERR_NAME);
    ++p->s;skip(p);
    if(fn==FN_IF) {
        a=expression(p);skip(p);
        if(*p->s!=',') { fail(p,SHEET_ERR_PARSE);return 0; }
        ++p->s;oldactive=p->active;branch=a!=0;
        p->active=oldactive && branch;b=expression(p);skip(p);
        if(*p->s!=',') { p->active=oldactive;fail(p,SHEET_ERR_PARSE);return 0; }
        ++p->s;p->active=oldactive && !branch;v=expression(p);
        p->active=oldactive;skip(p);
        if(*p->s!=')') fail(p,SHEET_ERR_PARSE);else ++p->s;
        return branch ? b : v;
    }
    if(*p->s!=')') do {
        skip(p);probe=*p;
        if(fn<=FN_COUNT && reference(&probe,&c1,&r1)) {
            skip(&probe);
            if(*probe.s==':' || *probe.s==',' || *probe.s==')') {
                *p=probe;c2=c1;r2=r1;
                if(*p->s==':') { ++p->s;skip(p);if(!reference(p,&c2,&r2)) fail(p,SHEET_ERR_PARSE); }
                if(c1>c2) { c=c1;c1=c2;c2=c; }
                if(r1>r2) { r=r1;r1=r2;r2=r; }
                for(r=r1;r<=r2;++r) for(c=c1;c<=c2;++c) {
                    i=find_cell(c,r);
                    if(i==SHEET_MAX_CELLS || CELL(i).kind==SHEET_TEXT) continue;
                    if(fn==FN_COUNT) {
                        v=p->active ? evaluate(i,(unsigned char)(p->depth+1)) : 0;
                        if(!CELL(i).error) ++count;
                    } else { v=ref_value(p,c,r,0);accumulate(p,fn,v,&result,&count); }
                }
                ++args;skip(p);
                if(*p->s!=',') break;
                ++p->s;continue;
            }
        }
        v=expression(p);
        if(args==0) a=v;else if(args==1) b=v;
        if(fn<=FN_COUNT || fn==FN_AND || fn==FN_OR) accumulate(p,fn,v,&result,&count);
        ++args;skip(p);
        if(*p->s!=',') break;
        ++p->s;
    } while(*p->s);
    if(*p->s!=')') fail(p,SHEET_ERR_PARSE);else ++p->s;
    if(fn==FN_SUM || fn==FN_MIN || fn==FN_MAX) return result;
    if(fn==FN_COUNT) return (SheetNum)count*100L;
    if(fn==FN_AVERAGE) { if(!count) { fail(p,SHEET_ERR_DIV0);return 0; } return divide(p,result,(SheetNum)count*100L); }
    if(fn==FN_AND || fn==FN_OR) { if(!args) fail(p,SHEET_ERR_VALUE);return result; }
    if(fn==FN_ROUND || fn==FN_MOD || fn==FN_POWER) { if(args!=2) fail(p,SHEET_ERR_VALUE); }
    else if(args!=1) fail(p,SHEET_ERR_VALUE);
    if(fn==FN_ABS) return a<0 ? -a : a;
    if(fn==FN_ROUND) return round_number(p,a,b);
    if(fn==FN_INT) {
        result=a/100L*100L;
        if(a<0 && a%100L) result=add(p,result,-100L);
        return result;
    }
    if(fn==FN_NOT) return a ? 0 : 100;
    if(fn==FN_SQRT) return square_root(p,a);
    if(fn==FN_POWER) return power(p,a,b);
    if(fn==FN_MOD) {
        if(!b) { fail(p,SHEET_ERR_DIV0);return 0; }
        result=a%b;if(result && ((result<0)!=(b<0))) result+=b;return result;
    }
    return 0;
}

static SheetNum primary(Parser *p)
{
    SheetNum n;
    unsigned char col,row,len;
    char name[12];
    skip(p);
    if(p->depth>=MAX_DEPTH) { fail(p,SHEET_ERR_DEPTH);return 0; }
    if(*p->s=='(') {
        ++p->s;++p->depth;n=expression(p);--p->depth;skip(p);
        if(*p->s==')') ++p->s;else fail(p,SHEET_ERR_PARSE);return n;
    }
    if(isdigit((unsigned char)*p->s) || *p->s=='.') return number(p);
    if(reference(p,&col,&row)) return ref_value(p,col,row,1);
    if(isalpha((unsigned char)*p->s)) {
        len=0;
        while(isalpha((unsigned char)*p->s)) { if(len<11) name[len++]=(char)toupper((unsigned char)*p->s);++p->s; }
        name[len]=0;skip(p);
        if(*p->s=='(') { ++p->depth;n=function(p,name);--p->depth;return n; }
        if(!strcmp(name,"TRUE")) return 100;
        if(!strcmp(name,"FALSE")) return 0;
        fail(p,SHEET_ERR_NAME);return 0;
    }
    fail(p,SHEET_ERR_PARSE);if(*p->s) ++p->s;return 0;
}
static SheetNum unary(Parser *p)
{
    unsigned char neg;
    SheetNum n;
    skip(p);neg=0;
    while(*p->s=='+' || *p->s=='-') { if(*p->s=='-') neg=!neg;++p->s;skip(p); }
    n=primary(p);skip(p);
    while(*p->s=='%') { n=divide(p,n,10000L);++p->s;skip(p); }
    return neg ? -n : n;
}
static SheetNum exponent(Parser *p)
{
    SheetNum n,b;
    n=unary(p);skip(p);
    while(*p->s=='^') { ++p->s;b=unary(p);if(p->active) n=power(p,n,b);skip(p); }
    return n;
}
static SheetNum term(Parser *p)
{
    SheetNum n,b;
    char op;
    n=exponent(p);skip(p);
    while(*p->s=='*' || *p->s=='/') {
        op=*p->s++;b=exponent(p);
        if(p->active) n=op=='*' ? multiply(p,n,b) : divide(p,n,b);
        skip(p);
    }
    return n;
}
static SheetNum sum(Parser *p)
{
    SheetNum n,b;
    char op;
    n=term(p);skip(p);
    while(*p->s=='+' || *p->s=='-') {
        op=*p->s++;b=term(p);
        if(p->active) n=add(p,n,op=='+' ? b : -b);
        skip(p);
    }
    return n;
}
static SheetNum expression(Parser *p)
{
    SheetNum n,b;
    char op,op2;
    n=sum(p);skip(p);
    while(*p->s=='=' || *p->s=='<' || *p->s=='>') {
        op=*p->s++;op2=0;
        if(*p->s=='=' || (*p->s=='>' && op=='<')) op2=*p->s++;
        b=sum(p);
        if(op=='=') n=n==b ? 100 : 0;
        else if(op=='<') n=(op2=='>' ? n!=b : (op2=='=' ? n<=b : n<b)) ? 100 : 0;
        else n=(op2=='=' ? n>=b : n>b) ? 100 : 0;
        skip(p);
    }
    return n;
}
static unsigned char kind(const char *raw)
{
    Parser p;
    if(!*raw) return SHEET_EMPTY;
    if(*raw=='=') return SHEET_FORMULA;
    if(*raw=='\'') return SHEET_TEXT;
    p.s=raw;p.error=0;p.active=1;p.depth=0;
    if(*p.s=='+' || *p.s=='-') ++p.s;
    number(&p);skip(&p);
    return !*p.s && !p.error ? SHEET_NUMBER : SHEET_TEXT;
}
static SheetNum evaluate(unsigned char index,unsigned char depth)
{
    Parser p;
    SheetNum n;
    Cell *cell;
    cell=&CELL(index);
    if(cell->state==2) return cell->value;
    if(cell->state==1) { cell->error=SHEET_ERR_CYCLE;return 0; }
    if(depth>=MAX_DEPTH) { cell->error=SHEET_ERR_DEPTH;return 0; }
    if(cell->kind==SHEET_TEXT) return 0;
    cell->state=1;
    p.s=cell->raw;p.error=0;p.active=1;p.depth=depth;
    if(cell->kind==SHEET_FORMULA) ++p.s;
    n=expression(&p);skip(&p);
    if(*p.s && !p.error) p.error=SHEET_ERR_PARSE;
    cell->error=p.error;cell->value=p.error ? 0 : n;cell->state=2;
    return cell->value;
}
void sheet_reset(void) { used=0; }
unsigned char sheet_set(unsigned char col,unsigned char row,const char *raw)
{
    unsigned char i,j;
    if(col>=SHEET_COLS || row>=SHEET_ROWS) return SHEET_ERR_REF;
    if(strlen(raw)>SHEET_TEXT_MAX) return SHEET_ERR_LONG;
    i=find_cell(col,row);
    if(!*raw) {
        if(i<used) { --used;if(i<used) CELL(i)=CELL(used); }
    } else {
        if(i==SHEET_MAX_CELLS) {
            if(used==SHEET_MAX_CELLS) return SHEET_ERR_FULL;
            i=used++;CELL(i).col=col;CELL(i).row=row;
        }
        strcpy(CELL(i).raw,raw);CELL(i).kind=kind(raw);
    }
    for(j=0;j<used;++j) { CELL(j).state=0;CELL(j).error=0; }
    return SHEET_OK;
}
const char *sheet_get(unsigned char col,unsigned char row)
{
    unsigned char i;i=find_cell(col,row);return i<used ? CELL(i).raw : "";
}
void sheet_recalc(void)
{
    unsigned char i;
    for(i=0;i<used;++i) { CELL(i).state=0;CELL(i).error=0; }
    for(i=0;i<used;++i) evaluate(i,0);
}
SheetNum sheet_value(unsigned char col,unsigned char row)
{
    unsigned char i;
    if(col>=SHEET_COLS || row>=SHEET_ROWS) return 0;
    i=find_cell(col,row);return i<used ? evaluate(i,0) : 0;
}
unsigned char sheet_error(unsigned char col,unsigned char row)
{
    unsigned char i;
    if(col>=SHEET_COLS || row>=SHEET_ROWS) return SHEET_ERR_REF;
    i=find_cell(col,row);if(i==SHEET_MAX_CELLS) return SHEET_OK;
    evaluate(i,0);return CELL(i).error;
}
unsigned char sheet_kind(unsigned char col,unsigned char row)
{
    unsigned char i;i=find_cell(col,row);return i<used ? CELL(i).kind : SHEET_EMPTY;
}
const char *sheet_error_text(unsigned char error)
{
    static const char * const names[]={"","#REF!","#VALUE!","#DIV/0!","#NUM!","#NAME?","#CYCLE!","FULL","TOO LONG","#DEPTH!","#SYNTAX!"};
    if(error<=SHEET_ERR_PARSE) return names[error];
    return "#ERROR!";
}
void sheet_format(SheetNum value,char *out)
{
    uint32_t n,whole;
    char rev[12];
    unsigned char len;
    if(value<0) *out++='-';
    n=magnitude(value);whole=n/100;len=0;
    do { rev[len++]=(char)('0'+whole%10);whole/=10; } while(whole);
    while(len) *out++=rev[--len];
    if(n%100) { *out++='.';*out++=(char)('0'+n%100/10);if(n%10) *out++=(char)('0'+n%10); }
    *out=0;
}
unsigned char sheet_count(void) { return used; }
const char *sheet_at(unsigned char index,unsigned char *col,unsigned char *row)
{
    if(index>=used) return 0;
    *col=CELL(index).col;*row=CELL(index).row;return CELL(index).raw;
}
unsigned char sheet_paste(const char *raw,unsigned char sc,unsigned char sr,unsigned char dc,unsigned char dr)
{
    const char *s,*start;
    char out[SHEET_TEXT_MAX+1],part[8];
    unsigned char n,k,ca,ra,c,r;
    int newc,newr;
    Parser p;
    if(sc>=SHEET_COLS || sr>=SHEET_ROWS || dc>=SHEET_COLS || dr>=SHEET_ROWS) return SHEET_ERR_REF;
    if(strlen(raw)>SHEET_TEXT_MAX) return SHEET_ERR_LONG;
    s=raw;
    if(*s!='=') { strcpy(out,s);return sheet_set(dc,dr,out); }
    n=0;out[n++]=*s++;
    while(*s) {
        start=s;p.s=s;p.error=0;p.active=1;p.depth=0;
        if((s==raw+1 || !isalnum((unsigned char)s[-1])) && reference(&p,&c,&r)) {
            if(p.error) return p.error;
            ca=*s=='$';if(ca) ++s;
            while(isalpha((unsigned char)*s)) ++s;
            ra=*s=='$';newc=(int)c+(ca ? 0 : (int)dc-sc);newr=(int)r+(ra ? 0 : (int)dr-sr);
            if(newc<0 || newc>=SHEET_COLS || newr<0 || newr>=SHEET_ROWS) return SHEET_ERR_REF;
            k=0;if(ca) part[k++]='$';part[k++]=(char)('A'+newc);if(ra) part[k++]='$';
            ++newr;if(newr==100) part[k++]='1';if(newr>=10) part[k++]=(char)('0'+(newr/10)%10);part[k++]=(char)('0'+newr%10);
            if(n+k>SHEET_TEXT_MAX) return SHEET_ERR_LONG;
            memcpy(out+n,part,k);n+=k;s=p.s;
        } else {
            s=start;if(n>=SHEET_TEXT_MAX) return SHEET_ERR_LONG;out[n++]=*s++;
        }
    }
    out[n]=0;return sheet_set(dc,dr,out);
}

unsigned char sheet_copy(unsigned char sc,unsigned char sr,unsigned char dc,unsigned char dr)
{
    return sheet_paste(sheet_get(sc,sr),sc,sr,dc,dr);
}

I.  Codul initial (C)

int fact(int n){
    if(n<3)return n;
    return n*fact(n-1);
}

void main(){
    put_i(4.9);       // => 4
    put_i(fact(3));   // => 6
    int r;
    r=1;
    int i;
    i=2;
    while(i<5){
        r=r*i;
        i=i+1;
    }
    put_i(r);         // => 24
}

II. Cadrul functiei
a. functia fact:
-2       -1         0
 n    adr_return  old_FP

b. functia main:
     -1       0      1    2
 adr_return old_FP   r    i

III. Functiile translatate (ASM)
markdownI.  Codul initial (C)

int fact(int n){
    if(n<3)return n;
    return n*fact(n-1);
}

void main(){
    put_i(4.9);       // => 4
    put_i(fact(3));   // => 6
    int r;
    r=1;
    int i;
    i=2;
    while(i<5){
        r=r*i;
        i=i+1;
    }
    put_i(r);         // => 24
}

II. Cadrul functiei

a. functia fact:
  -2       -1         0
   n    adr_return  old_FP

fact are 0 variabile locale, deci ENTER 0.
n este parametru int, accesat la FP[-2].

b. functia main:
   -1         0       1       2
adr_return  old_FP   r       i

main are 2 variabile locale (r si i), deci ENTER 2.
main nu are parametri.

III. Functiile translatate (ASM)

     CALL main           <- push adr(HALT), sari la ENTER
     HALT                <- executia se opreste dupa return

; ─── void main() ──────────────────────────────────────────
main: ENTER 2            <- push old_FP, FP=SP, SP+=2 (r la FP[1], i la FP[2])

     ; put_i(4.9)
     ; 4.9 este DOUBLE, put_i asteapta INT -> insertConvIfNeeded emite CONV.f.i
     PUSH.f 4.9          <- valoarea argumentului 4.9
     CONV.f.i            <- converteste 4.9 -> 4 (int)
     CALL_EXT put_i      <- afiseaza 4

     ; put_i(fact(3))
     PUSH.i 3            <- argumentul pentru fact
     CALL fact           <- push adr(urm. instructiune), sare la fact
     CALL_EXT put_i      <- afiseaza 6

     ; r=1
     FPADDR.i 1          <- push adresa FP[1] (r, destinatia)
     PUSH.i 1            <- valoarea 1
     STORE.i             <- *(int*)FP[1] = 1; lasa 1 pe stiva
     DROP                <- rezultatul atribuirii e aruncat

     ; i=2
     FPADDR.i 2          <- push adresa FP[2] (i, destinatia)
     PUSH.i 2            <- valoarea 2
     STORE.i             <- *(int*)FP[2] = 2; lasa 2 pe stiva
     DROP                <- rezultatul atribuirii e aruncat

     ; while(i<5)
W1:  FPADDR.i 2          <- push adresa i (stanga)
     LOAD.i              <- push valoarea lui i
     PUSH.i 5            <- push 5 (dreapta)
     LESS.i              <- i < 5 -> int 0 sau 1
     JF W2               <- daca fals, iesi din bucla

     ; r=r*i
     FPADDR.i 1          <- push adresa r (destinatia)
     FPADDR.i 1          <- push adresa r (stanga MUL)
     LOAD.i              <- push valoarea lui r
     FPADDR.i 2          <- push adresa i (dreapta MUL)
     LOAD.i              <- push valoarea lui i
     MUL.i               <- r * i
     STORE.i             <- *(int*)FP[1] = r*i; lasa rezultatul pe stiva
     DROP                <- rezultatul atribuirii e aruncat

     ; i=i+1
     FPADDR.i 2          <- push adresa i (destinatia)
     FPADDR.i 2          <- push adresa i (stanga ADD)
     LOAD.i              <- push valoarea lui i
     PUSH.i 1            <- push 1 (dreapta ADD)
     ADD.i               <- i + 1
     STORE.i             <- *(int*)FP[2] = i+1; lasa rezultatul pe stiva
     DROP                <- rezultatul atribuirii e aruncat

     JMP W1              <- inapoi la conditia while

W2:  NOP

     ; put_i(r)
     FPADDR.i 1          <- push adresa r
     LOAD.i              <- push valoarea lui r (24)
     CALL_EXT put_i      <- afiseaza 24

     RET_VOID 0          <- return; 0 parametri, restaureaza FP/SP/IP

; ─── int fact(int n) ──────────────────────────────────────
fact: ENTER 0            <- push old_FP, FP=SP, SP+=0 (fara variabile locale)

     ; if(n<3)
     FPADDR.i -2         <- push adresa n (stanga)
     LOAD.i              <- push valoarea lui n
     PUSH.i 3            <- push 3 (dreapta)
     LESS.i              <- n < 3 -> int 0 sau 1
     JF F1               <- daca fals (n>=3), sari la return n*fact(n-1)

     ; return n
     FPADDR.i -2         <- push adresa n
     LOAD.i              <- push valoarea lui n
     RET 1               <- return n; restaureaza frame, 1 parametru

F1:  NOP

     ; return n*fact(n-1)
     FPADDR.i -2         <- push adresa n (stanga MUL)
     LOAD.i              <- push valoarea lui n
     FPADDR.i -2         <- push adresa n (pentru SUB)
     LOAD.i              <- push valoarea lui n
     PUSH.i 1            <- push 1
     SUB.i               <- n - 1
     CALL fact           <- push ret addr, recursie cu n-1
     MUL.i               <- n * fact(n-1)
     RET 1               <- return rezultat; 1 parametru
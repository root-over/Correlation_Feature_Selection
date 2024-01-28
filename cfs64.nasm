; ---------------------------------------------------------
; Regression con istruzioni AVX a 64 bit
; ---------------------------------------------------------
; F. Angiulli
; 23/11/2017
;

;
; Software necessario per l'esecuzione:
;
;     NASM (www.nasm.us)
;     GCC (gcc.gnu.org)
;
; entrambi sono disponibili come pacchetti software 
; installabili mediante il packaging tool del sistema 
; operativo; per esempio, su Ubuntu, mediante i comandi:
;
;     sudo apt-get install nasm
;     sudo apt-get install gcc
;
; potrebbe essere necessario installare le seguenti librerie:
;
;     sudo apt-get install lib32gcc-4.8-dev (o altra versione)
;     sudo apt-get install libc6-dev-i386
;
; Per generare file oggetto:
;
;     nasm -f elf64 regression64.nasm
;

%include "sseutils64.nasm"

section .data			; Sezione contenente dati inizializzati

section .bss			; Sezione contenente dati non inizializzati

alignb 32
sc		resq		1

section .text			; Sezione contenente il codice macchina

; ----------------------------------------------------------
; macro per l'allocazione dinamica della memoria
;
;	getmem	<size>,<elements>
;
; alloca un'area di memoria di <size>*<elements> bytes
; (allineata a 16 bytes) e restituisce in EAX
; l'indirizzo del primo bytes del blocco allocato
; (funziona mediante chiamata a funzione C, per cui
; altri registri potrebbero essere modificati)
;
;	fremem	<address>
;
; dealloca l'area di memoria che ha inizio dall'indirizzo
; <address> precedentemente allocata con getmem
; (funziona mediante chiamata a funzione C, per cui
; altri registri potrebbero essere modificati)

extern get_block
extern free_block

%macro	getmem	2
	mov	rdi, %1
	mov	rsi, %2
	call	get_block
%endmacro

%macro	fremem	1
	mov	rdi, %1
	call	free_block
%endmacro

; ------------------------------------------------------------
; Funzione prova
; ------------------------------------------------------------
global prova
global calcola_rff_new

msg	db 'sc:',32,0
nl	db 10,0

section .text

calcola_rff_new:
    start
    ; Carica gli argomenti nei registri
    ;ds = RDI
    ;N = RSI
    ;fx = RDX
    ;fy = RCX
    ;media_elem_fx = xmm0
    ;media_elem_fy = xmm1
    ;ret = R8

    ; Inizializza le variabili locali (sommatoria1, sommatoria2, sommatoria3)
    vxorps xmm2, xmm2, xmm2 ; Azzera xmm2, xmm3, xmm4 per le sommatorie
    vxorps xmm3, xmm3, xmm3
    vxorps xmm4, xmm4, xmm4

    ; Calcola inizio_fx = fx * N
    imul rdx, rsi           ; rsi = fx * N
    imul rcx, rsi           ; rdx = fy * N

    ; Preparazione per il ciclo
    xor rax, rax            ; i = 0

inizio_ciclo:
    cmp rax, rsi            ; Confronta i con N
    jge fine_ciclo          ; Se i >= N, salta a fine_ciclo

    ; Calcola operazione1 = ds[inizio_fx + i] - mux
    mov rbx, rdx            ; Copia inizio_fx in rbx
    add rbx, rax            ; Aggiungi i a inizio_fx (rax = inizio_fx + i)
    vmovss xmm5, [rdi + rbx*8] ; Carica ds[inizio_fx + i] in xmm5
    ;FIXME sbagliato perche dentro xmm0 c'è il valore sbagliato (il calcolo è giusto)
    vsubss xmm5, xmm5, xmm0 ; xmm5 = xmm5 - mux (operazione1)

    ; Calcola operazione2 = ds[inizio_fy + i] - muy
    mov rbx, rcx            ; Carica fy in rbx
    add rbx, rax            ; Aggiungi i a inizio_fy (rax = inizio_fy + i)
    vmovss xmm6, [rdi + rbx*8] ; Carica ds[inizio_fy + i] in xmm6
    vsubss xmm6, xmm6, xmm1 ; xmm6 = xmm6 - muy (operazione2)

    ; Calcola sommatoria1 += ((operazione1) * (operazione2))
    vmovss xmm7, xmm5       ; xmm7 = xmm5 (ds[fx])
    vmulss xmm7, xmm7, xmm6 ; xmm7 = operazione1 * operazione2
    vaddss xmm2, xmm2, xmm7 ; sommatoria1 += xmm7

    ; Calcola sommatoria2 += ((operazione1) * (operazione1))
    vmulss xmm5, xmm5, xmm5 ; xmm5 = operazione1 * operazione1
    vaddss xmm3, xmm3, xmm5 ; sommatoria2 += xmm5

    ; Calcola sommatoria3 += ((operazione2) * (operazione2))
    vmulss xmm6, xmm6, xmm6 ; xmm6 = operazione2 * operazione2
    vaddss xmm4, xmm4, xmm6 ; sommatoria3 += xmm6

    ; Incrementa il contatore del ciclo
    inc rax
    jmp inizio_ciclo
fine_ciclo:

    ; Calcola sqrtf(sommatoria2 * sommatoria3)
    vmulss xmm3, xmm3, xmm4 ; xmm3 = sommatoria2 * sommatoria3
    vsqrtss xmm3, xmm3, xmm3 ; xmm3 = sqrt(sommatoria2 * sommatoria3)

    ; Calcola sommatoria1 / sqrt(sommatoria2 * sommatoria3)
    vdivss xmm2, xmm2, xmm3 ; xmm2 = sommatoria1 / sqrt(sommatoria2 * sommatoria3)

    ; Prepara il valore di ritorno
    vmovss [r8], xmm2
    stop

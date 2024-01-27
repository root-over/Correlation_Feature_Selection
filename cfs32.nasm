; ---------------------------------------------------------
; Regressione con istruzioni SSE a 32 bit
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
;     nasm -f elf32 fss32.nasm 
;
%include "sseutils32.nasm"

global f_max
global gap

section .data			; Sezione contenente dati inizializzati
     hello db 'Hello, World!',0
     f_max dd 0  ; Inizializziamo f_max a 0
     gap dd 0    ; Inizializziamo gap a 0
     mux dd 0
     muy dd 0
     valore_assoluto_mask dd 7FFFFFFFh ; Maschera per il valore assoluto (elimina il bit di segno)


section .bss			; Sezione contenente dati non inizializzati
	alignb 16
	sc		resd		1

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
global calcola_rff_new

%macro	getmem	2
	mov	eax, %1
	push	eax
	mov	eax, %2
	push	eax
	call	get_block
	add	esp, 8
%endmacro

%macro	fremem	1
	push	%1
	call	free_block
	add	esp, 4
%endmacro

; ------------------------------------------------------------
; Funzioni
; ------------------------------------------------------------


input		equ		8

msg	db	'sc:',32,0
nl	db	10,0

calcola_rff_new:
    start
    ; Carica gli argomenti nei registri
    mov esi, [ebp+8]          ; Puntatore ds
    mov edx, [ebp+12]         ; N
    mov ebx, [ebp+16]         ; fx
    mov eax, [ebp+20]         ; fy
    movss xmm0, [ebp+24]      ; media_elem_fx -> xmm0
    movss xmm1, [ebp+28]      ; media_elem_fy -> xmm1

    ; Inizializza le variabili locali (sommatoria1, sommatoria2, sommatoria3)
    xorps xmm2, xmm2          ; Azzera xmm2, xmm3, xmm4 per le sommatorie
    xorps xmm3, xmm3
    xorps xmm4, xmm4

    ; Calcola inizio_fx = fx * N
    imul ebx, edx             ; ebx = fx * N
    mov ecx, eax
    imul ecx, edx

    ; Preparazione per il ciclo
    xor edi, edi              ; i = 0

inizio_ciclo:
    cmp edi, edx              ; Confronta i con N
    jge fine_ciclo            ; Se i >= N, salta a fine_ciclo

; Calcola operazione1 = ds[inizio_fx + i] - mux
    mov eax, ebx              ; Copia inizio_fx in eax
    add eax, edi              ; Aggiungi i a inizio_fx (eax = inizio_fx + i)
    movss xmm5, [esi + eax*4] ; Carica ds[inizio_fx + i] in xmm5 (presumendo che type sia float)
    subss xmm5, xmm0          ; xmm5 = xmm5 - mux (operazione1)

    ; Calcola operazione2 = ds[inizio_fy + i] - muy
    mov eax, ecx              ; Carica fy in eax
    add eax, edi              ; Aggiungi i a inizio_fy (eax = inizio_fy + i)
    movss xmm6, [esi + eax*4] ; Carica ds[inizio_fy + i] in xmm6
    subss xmm6, xmm1          ; xmm6 = xmm6 - muy (operazione2)

    ; Calcola sommatoria1 += ((operazione1) * (operazione2))
    movss xmm7, xmm5          ; xmm7 = xmm5 (ds[fx])
    mulss xmm7, xmm6          ; xmm7 = operazione1 * operazione2
    addss xmm2, xmm7          ; sommatoria1 += xmm5

    ; Calcola sommatoria2 += ((operazione1) * (operazione1))
    mulss xmm5, xmm5          ; xmm5 = operazione1 * operazione1
    addss xmm3, xmm5          ; sommatoria2 += xmm5

    ; Calcola sommatoria3 += ((operazione2) * (operazione2))
    mulss xmm6, xmm6          ; xmm6 = operazione2 * operazione2
    addss xmm4, xmm6          ; sommatoria3 += xmm6

    ; Incrementa il contatore del ciclo
    inc edi
    jmp inizio_ciclo
fine_ciclo:

; Calcola sqrtf(sommatoria2 * sommatoria3)
    mulss xmm3, xmm4          ; xmm3 = sommatoria2 * sommatoria3
    sqrtss xmm3, xmm3         ; xmm3 = sqrt(sommatoria2 * sommatoria3)

    ; Calcola sommatoria1 / sqrt(sommatoria2 * sommatoria3)
    divss xmm2, xmm3          ; xmm2 = sommatoria1 / sqrt(sommatoria2 * sommatoria3)

    ; Prepara il valore di ritorno
    mov eax,[ebp + 32]       ; Memorizza il risultato nello stack per il ritorno
    movss [eax], xmm2
    stop
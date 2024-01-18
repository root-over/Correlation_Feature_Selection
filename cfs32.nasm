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
global hello_world
global calcola_rff

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

global prova


input		equ		8

msg	db	'sc:',32,0
nl	db	10,0

hello_world:
    ; Write the string to stdout
    mov eax, 4           ; syscall: sys_write
    mov ebx, 1           ; file descriptor: stdout
    mov ecx, hello       ; pointer to the string
    mov edx, 13          ; length of the string
    int 0x80             ; interrupt to invoke the syscall

    ; Exit the function
    ret


calcola_rff:
    mov eax, [esp + 4]         ; ds (float*)
    mov ebx, [esp + 8]         ; N
    mov ecx, [esp + 12]        ; d
    mov edx, [esp + 16]        ; fx
    mov esi, [esp + 20]        ; pos_fy
    mov edi, [esp + 24]        ; media_elem (flaot*)
    mov ebp, [esp + 28]        ; out (int*)

    ; Calcola fy
    movzx eax, word [ebp + esi * 4] ; ds[pos_fy] (fy)
    movzx ebx, word [edi + edx * 4]  ; media_elem[fx]
    movzx ecx, word [edi + eax * 4]  ; media_elem[fy]

    ; Inizializza sommatorie
    xorps xmm0, xmm0  ; sommatoria1
    xorps xmm1, xmm1  ; sommatoria2
    xorps xmm2, xmm2  ; sommatoria3

    ; Calcolare f_max e gap
    cmp edx, eax
    jg greater_than
    ; fx <= fy
    mov eax, ebx
    sub eax, ecx
    mov [f_max], eax
    mov eax, ecx
    sub eax, ebx
    mov [gap], eax
    jmp start_loop

greater_than:
    ; fx > fy
    mov eax, ecx
    sub eax, ebx
    mov [f_max], eax
    mov eax, ebx
    sub eax, ecx
    mov [gap], eax


start_loop:
    ; Inizializzo il contatore del loop
    mov ecx, f_max

main_loop:
    ; Calcolo l'indice dell'array
    imul edx, ebx
    add eax, edx
    add ebx, eax
    sub ebx, [gap]

    ; Carico i dati dalla memoria
    movss xmm3, [esi + eax * 4]  ; ds[i]
    movss xmm4, [esi + ebx * 4]  ; ds[i - gap]

    ;FIXME il segmentatio fault avviene qua
    ; Sottraggo mux e muy
    subss xmm3, xmm0  ; operazione1 = ds[i] - mux
    subss xmm4, xmm2  ; operazione2 = ds[i - gap] - muy

    ; Moltiplico
    mulss xmm1, xmm3  ; operazione1 * operazione2
    mulss xmm2, xmm4  ; operazione1 * operazione1
    mulss xmm3, xmm3  ; operazione2 * operazione2

    ; sommo
    addss xmm0, xmm3  ; sommatoria1 += operazione1 * operazione2
    addss xmm1, xmm2  ; sommatoria2 += operazione1 * operazione1
    addss xmm2, xmm3  ; sommatoria3 += operazione2 * operazione2

    ; Mi muovo alla prossima iterazione
    add ecx, ebx
    cmp ecx, [esp + 4]
    jl main_loop

    ; Calcolo il risultato
    sqrtss xmm1, xmm1  ; sqrt(sommatoria2)
    mulss xmm1, xmm2   ; sqrt(sommatoria2 * sommatoria3)

    ; calcolo fabs(sommatoria1 / (sommatoria2 * sommatoria3))
    divss xmm0, xmm1
    movss xmm1, xmm0
    andps xmm1, xmm_size [abs_mask]
    movss [esp], xmm1  ; Memorizzo il risultato nella prima dword dello stack

    ; Pulisco e return
    fld dword [esp]
    add esp, 4
    ret

section .data
abs_mask dd 0x7FFFFFFF  ; Maschera per pulire i registri xmm
xmm_size equ 16  ; Dimensione in byte di un registro XMM (128 bit)

prova:
		; ------------------------------------------------------------
		; Sequenza di ingresso nella funzione
		; ------------------------------------------------------------
		push		ebp		; salva il Base Pointer
		mov		ebp, esp	; il Base Pointer punta al Record di Attivazione corrente
		push		ebx		; salva i registri da preservare
		push		esi
		push		edi
		; ------------------------------------------------------------
		; legge i parametri dal Record di Attivazione corrente
		; ------------------------------------------------------------

		; elaborazione

		; esempio: stampa input->sc
		mov EAX, [EBP+input]	; indirizzo della struttura contenente i parametri
        ; [EAX] input->ds; 			// dataset
		; [EAX + 4] input->labels; 	// etichette
		; [EAX + 8] input->out;	// vettore contenente risultato dim=(k+1)
		; [EAX + 12] input->sc;		// score dell'insieme di features risultato
		; [EAX + 16] input->k; 		// numero di features da estrarre
		; [EAX + 20] input->N;		// numero di righe del dataset
		; [EAX + 24] input->d;		// numero di colonne/feature del dataset
		; [EAX + 28] input->display;
		; [EAX + 32] input->silent;
		MOVSS XMM0, [EAX+12]
		MOVSS [sc], XMM0
		prints msg
		printss sc
		prints nl
		; ------------------------------------------------------------
		; Sequenza di uscita dalla funzione
		; ------------------------------------------------------------

		pop	edi		; ripristina i registri da preservare
		pop	esi
		pop	ebx
		mov	esp, ebp	; ripristina lo Stack Pointer
		pop	ebp		; ripristina il Base Pointer
		ret			; torna alla funzione C chiamante

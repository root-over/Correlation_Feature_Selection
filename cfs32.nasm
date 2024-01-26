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
global hello_world
;global calcola_rff
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

global prova


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


    ; Qui potrebbe essere necessaria ulteriore configurazione...

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
    mov eax, [ebp+20]         ; Carica fy in eax
    imul eax, edx             ; eax = fy * N
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

    ; Calcola il valore assoluto (fabsf)
    ;andps xmm2, [valore_assoluto_mask] ; Rimuovi il segno da xmm2

    ; Prepara il valore di ritorno
    mov eax,[ebp + 32]       ; Memorizza il risultato nello stack per il ritorno
    movss [eax], xmm2
    stop

calcola_rff:
    start
    mov eax, [ebp + 8]         ; ds (float*)
    mov ebx, [ebp + 12]           ; N
    ;[ebp + 16] d
    mov ecx, [ebp + 20]           ; fx
    mov edx, [ebp + 24]           ; fy
    movss xmm3, [ebp + 28]        ; media_elem_fx
    movss xmm4, [ebp + 32]        ; media_elem_fy

    imul ebx, [ebp + 16]            ; N x d (grandezza della matrice)
;cvtsi2ss xmm1, eax (converte)

    ; Inizializza sommatorie
    xorps xmm0, xmm0  ; sommatoria1
    xorps xmm1, xmm1  ; sommatoria2
    xorps xmm2, xmm2  ; sommatoria3

    ; Calcolare f_max e gap
    cmp ecx, edx
    jg greater_than
    ; fx <= fy
    mov [f_max], edx ; metto dentro f_max fy TODO attualmente creo una variabile globale ma meglio usare un registro
    sub edx, ecx ; calcolo il gap
    mov [gap], edx; assegno il gap alla variabile globale apposita TODO attualmente creo una variabile globale ma meglio usare un registro
    movss xmm5, xmm4 ; muy
    movss xmm4, xmm3 ; mux
    jmp start_loop

greater_than:
    ; fx > fy
    mov [f_max], ecx ;metto dentro f_max fx TODO attualmente creo una variabile globale ma meglio usare un registro
    sub ecx, edx ; calcolo il gap
    mov [gap], ecx ;assegno il gap alla variabile globale apposita FIXME attualmente creo una variabile globale ma meglio usare un registro
    movss xmm5, xmm3 ; mux
    movss xmm4, xmm4 ;muy

start_loop:
    ; Inizializzo il contatore del loop
    mov ecx, [f_max]

main_loop:
    cmp ecx, ebx
    jge exit_loop
    ; Calcolo l'indice dell'array
    mov esi, ecx ; i
    mov edi, [gap]
    sub esi, edi ; i - gap

    ; Carico i dati dalla memoria
    movss xmm6, [eax + ecx * 4]  ; ds[i]
    movss xmm7, [eax + esi * 4]  ; ds[i - gap]

    ; Sottraggo mux e muy
    subss xmm6, xmm5  ; operazione1 = ds[i] - mux/muy
    subss xmm7, xmm4  ; operazione2 = ds[i - gap] - muy/mux

    ; inizializzo registri e moltiplico

    movss xmm5, xmm6
    mulss xmm5, xmm7  ;operazione1 * operazione2
    addss xmm0, xmm5  ; sommatoria1 += operazione1 * operazione2

    movss xmm5, xmm6
    mulss xmm5, xmm5 ; operazione1 * operazione1
    addss xmm1, xmm5 ; sommatoria2 += operazione1 * operazione1

    movss xmm5, xmm7
    mulss xmm5, xmm5 ; operazione2 * operazione2
    addss xmm2, xmm5 ; sommatoria3 += operazione2 * operazione2

    add ecx,[ebp + 16]

    jmp main_loop

exit_loop:
    ; Calcolo il risultato
    mulss xmm1, xmm2   ; sommatoria2 * sommatoria3
    sqrtss xmm1, xmm1  ; sqrt(sommatoria2)

    ; calcolo fabs(sommatoria1 / (sommatoria2 * sommatoria3))
    divss xmm0, xmm1

    ; Pulisco e return
    mov eax,[ebp + 36]
    movss [eax], xmm0
    stop

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

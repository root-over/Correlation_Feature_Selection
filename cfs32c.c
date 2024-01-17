/**************************************************************************************
* 
* CdL Magistrale in Ingegneria Informatica
* Corso di Architetture e Programmazione dei Sistemi di Elaborazione - a.a. 2020/21
* 
* Progetto dell'algoritmo Attention mechanism 221 231 a
* in linguaggio assembly x86-32 + SSE
* 
* Fabrizio Angiulli, novembre 2022
* 
**************************************************************************************/

/*
* 
* Software necessario per l'esecuzione:
* 
*    NASM (www.nasm.us)
*    GCC (gcc.gnu.org)
* 
* entrambi sono disponibili come pacchetti software 
* installabili mediante il packaging tool del sistema 
* operativo; per esempio, su Ubuntu, mediante i comandi:
* 
*    sudo apt-get install nasm
*    sudo apt-get install gcc
* 
* potrebbe essere necessario installare le seguenti librerie:
* 
*    sudo apt-get install lib32gcc-4.8-dev (o altra versione)
*    sudo apt-get install libc6-dev-i386
* 
* Per generare il file eseguibile:
* 
* nasm -f elf32 att32.nasm && gcc -m32 -msse -O0 -no-pie sseutils32.o att32.o att32c.c -o att32c -lm && ./att32c $pars
* 
* oppure
* 
* ./runatt32
* 
*/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <xmmintrin.h>

#define    type        float
#define    MATRIX        type*
#define    VECTOR        type*

typedef struct {
    MATRIX ds;        // dataset
    VECTOR labels;    // etichette
    int *out;        // vettore contenente risultato dim=k
    int dim;        // Valori inseriti all'interno di out
    type rff_totale;// L'rff totale calcolato
    MATRIX rff;       // Matrice contenente gli rff di ogni feature fx x fy
    type sc;        // score dell'insieme di features risultato
    int k;            // numero di features da estrarre
    int N;            // numero di righe del dataset
    int d;            // numero di colonne/feature del dataset
    int display;
    int silent;
} params;

/*
*	Le funzioni sono state scritte assumento che le matrici siano memorizzate 
* 	mediante un array (float*), in modo da occupare un unico blocco
* 	di memoria, ma a scelta del candidato possono essere 
* 	memorizzate mediante array di array (float**).
* 
* 	In entrambi i casi il candidato dovr� inoltre scegliere se memorizzare le
* 	matrici per righe (row-major order) o per colonne (column major-order).
*
* 	L'assunzione corrente � che le matrici siano in row-major order.
* 
*/

//riserva un blocco di memoria
void *get_block(int size, int elements) {
    return _mm_malloc(elements * size, 32);
    //è generalmente utilizzata per allocare memoria allineata
    //in modo specifico per migliorare le prestazioni delle istruzioni SIMD.
}

//libera il blocco di memoria
void free_block(void *p) {
    _mm_free(p);
}

//Alloca una matrice di float* (MATRIX)
MATRIX alloc_matrix(int rows, int cols) {
    return (MATRIX) get_block(sizeof(type), rows * cols);
}

//Alloca una matrice di int*
int *alloc_int_matrix(int rows, int cols) {
    return (int *) get_block(sizeof(int), rows * cols);
}

//Dealloca le matrici
void dealloc_matrix(void *mat) {
    free_block(mat);
}

/*
*
* 	load_data
* 	=========
*
*	Legge da file una matrice di N righe
* 	e M colonne e la memorizza in un array lineare in row-major order
*
* 	Codifica del file:
* 	primi 4 byte: numero di righe (N) --> numero intero
* 	successivi 4 byte: numero di colonne (M) --> numero intero
* 	successivi N*M*4 byte: matrix data in row-major order --> numeri floating-point a precisione singola
*
*****************************************************************************
*	Se lo si ritiene opportuno, � possibile cambiare la codifica in memoria
* 	della matrice.
*****************************************************************************
*
*/
MATRIX load_data(char *filename, int *n, int *k) {
    FILE *fp;
    int rows, cols;

    fp = fopen(filename, "rb"); //apre il file in lettura binaria

    if (fp == NULL) {
        printf("'%s': nome del file errato!\n", filename);
        exit(0);
    }


    //leggono rispettivamente la dimensione della riga e della colonna della matrice mettendoli nelle apposite variabili
    fread(&cols, sizeof(int), 1, fp);
    //ptr: puntatore al blocco di memoria in cui verranno memorizzati i dati
    //size: dimensione in byte dei dati
    //n: numero di elementi da leggere
    //stream: puntatore al file da cui leggere i dati

    fread(&rows, sizeof(int), 1, fp);

    //alloca la matrice "data" con dimensione rows e cols
    MATRIX data = alloc_matrix(rows, cols);

    //popola la matrice con gli elementi all'interno del file
    fread(data, sizeof(type), rows * cols, fp);
    fclose(fp);

    //assegna ai valori puntati da n e k il numero di righe e il numero di colonne
    *n = rows;
    *k = cols;

    return data;
}

/*
* 	save_data
* 	=========
*
*	Salva su file un array lineare in row-major order
*	come matrice di N righe e M colonne
*
* 	Codifica del file:
* 	primi 4 byte: numero di righe (N) --> numero intero a 32 bit
* 	successivi 4 byte: numero di colonne (M) --> numero intero a 32 bit
* 	successivi N*M*4 byte: matrix data in row-major order --> numeri interi o floating-point a precisione singola
*/
void save_data(char *filename, void *X, int n, int k) {

    //X rappresenta la matrice di dati linearizzata (return della funzione load_data)

    FILE *fp;
    int i;
    fp = fopen(filename, "wb"); //apre il file in lettura binaria
    //Se in X è presente qualcosa imposta le dimensioni della matrice nel file
    if (X != NULL) {
        //k righe, n colonne
        fwrite(&k, 4, 1, fp);
        fwrite(&n, 4, 1, fp);
        //copia gli elementi di X nel file
        for (i = 0; i < n; i++) {
            fwrite(X, sizeof(type), k, fp);
            //printf("%i %i\n", ((int*)X)[0], ((int*)X)[1]);
            //sposta il puntatore di X alla colonna successiva della matrice
            X += sizeof(type) * k;
        }
    }
        //Altrimenti imposta nel file la matice a dimensione 0
    else {
        int x = 0;
        fwrite(&x, 4, 1, fp);
        fwrite(&x, 4, 1, fp);
    }
    fclose(fp);
}

/*
* 	save_out
* 	=========
*
*	Salva su file un array lineare composto da k+1 elementi.
*
* 	Codifica del file:
* 	primi 4 byte: contenenti l'intero 1 		--> numero intero a 32 bit
* 	successivi 4 byte: numero di elementi (k+1) --> numero intero a 32 bit
* 	successivi byte: elementi del vettore 		--> 1 numero floating-point a precisione singola e k interi
*/
void save_out(char *filename, type sc, int *X, int k) {
    FILE *fp;
    int n = 1;
    k++;
    fp = fopen(filename, "wb");
    if (X != NULL) {
        //imposta dimensione righe e colonne della matrice linearizzata
        fwrite(&n, 4, 1, fp);
        fwrite(&k, 4, 1, fp);
        //scrive lo score dell'insieme di feature
        fwrite(&sc, sizeof(type), 1, fp);
        //svrive i dati della matrice linearizzata
        fwrite(X, sizeof(int), k, fp);
    }
    fclose(fp);
}

// PROCEDURE ASSEMBLY

extern void prova(params *input);

type media_valori_0_f(const type *ds, const type *labels, int N, int d, int f) {
    type somma = 0;
    int contatore = 0;
    int cont = 0;

    for (int i = f; i < N * d; i += d) {
        cont++;
        if (labels[i / d] == 0) {//Magaria per scorrere il vettore label attraverso l'indice del for
            somma = somma + ds[i];
            contatore++;
        }
    }

    return somma / (type) contatore;
}

type media_valori_1_f(const type *ds, const type *labels, int N, int d, int f) {
    type somma = 0;
    int contatore = 0;
    int cont = 0;

    for (int i = f; i < N * d; i += d) {
        cont++;
        if (labels[i / d] == 1) {
            somma = somma + ds[i];
            contatore++;
        }
    }
    return somma / (type) contatore;
}

//GIUSTO
int conta_elementi_1(const type *labels, int N) {
    int contatore = 0;
    for (int i = 0; i < (N); i++) {
        if (labels[i] == 0) {
            contatore++;
        }
    }
    return contatore;
}

//GIUSTO
type calcola_media(const type *ds, int N, int d, int f) {
    type somma = 0;
    for (int i = f; i < N * d; i += d) {
        somma += ds[i];
    }
    return somma / (type) (N);
}

//GIUSTO
type deviazione_standard_c(const type *ds, int N, int d, int f) {
    type media = calcola_media(ds, N, d, f);
    type somma_quadrati_diff = 0;

    for (int i = f; i < N * d; i += d) {
        type diff = ds[i] - media;
        somma_quadrati_diff += diff * diff;
    }
    return sqrtf(somma_quadrati_diff / (type) (N-1));
}

//GIUSTO
type calcola_rcf(const type *ds, const type *labels, int N, int d, int f, int n0, int n1) {
    type mu0 = media_valori_0_f(ds, labels, N, d, f);
    type mu1 = media_valori_1_f(ds, labels, N, d, f);

    int n = n0 + n1;
    type sf = deviazione_standard_c(ds, N, d, f);

    return fabsf((type) (((mu0 - mu1) / sf) * sqrtf((type)(n0 * n1) / (type)(n*n)))); //Rcf
}

//GIUSTO
type* calcola_max_rcf(params *input, const type *ds, int d, int N) {
    int f_rcf_max = 0;
    type rcf_max = 0;
    type *label = input->labels;

    type *rcf = (type *) malloc(d * sizeof(type));

    int n1 = conta_elementi_1(label, N);
    int n0 = N - n1;

    for (int i = 0; i < d; i++) {
        rcf[i] = calcola_rcf(ds, label, N, d, i, n0, n1);

        if (rcf[i] > rcf_max) {
            f_rcf_max = i;
            rcf_max = rcf[i];
        }
    }
    input->out[0] = f_rcf_max;
    input->dim = 1;
    return rcf;
}

//GIUSTO
type calcola_rff(const type *ds, int N, int d, int fx, int pos_fy, const type media_elem[], const int *out) {
    int fy = out[pos_fy];
    type mux = media_elem[fx];
    type muy = media_elem[fy];
    type sommatoria1 = 0;
    type sommatoria2 = 0;
    type sommatoria3 = 0;
    int f_max;
    int gap;

    //MIGLIORABILE
    if (fx > fy) {
        f_max = fx;
        gap = fx - fy;
        for (int i = f_max; i < N * d; i += d) {
            type operazione1 = ds[i] - mux;
            type operazione2 = ds[i - gap] - muy;
            sommatoria1 += ((operazione1) * (operazione2));
            sommatoria2 += ((operazione1) * (operazione1));
            sommatoria3 += ((operazione2) * (operazione2));
        }
    } else {
        f_max = fy;
        gap = fy - fx;
        for (int i = f_max; i < N * d; i += d) {
            type operazione1 = ds[i - gap] - mux;
            type operazione2 = ds[i] - muy;
            sommatoria1 += ((operazione1) * (operazione2));
            sommatoria2 += ((operazione1) * (operazione1));
            sommatoria3 += ((operazione2) * (operazione2));
        }
    }
    return fabsf((type) (sommatoria1 / (sqrtf(sommatoria2 * sommatoria3))));
}
//extern float calcola_rff(const float *ds, int N, int d, int fx, int pos_fy, const float media_elem[], const int *out);


type somma_rcf(const type *rcf, int dim, const int* out) {
    type rcf_tot = 0;
    for (int i = 0; i < dim; i++) {
        rcf_tot += rcf[out[i]];
    }
    return rcf_tot;
}

type calcola_merit(params *input, const type *ds, int N, int d, int f, type media_elem[], type *rcf, int *out,
                   type rff_totale, int dim, int coppie) {
    type rff_tot = 0;

    for (int i = 0; i < dim; i++) {
        //TODO invece di calcolare ogni volta l'rff calcolare tutti gli rff e metterli in una matrice dxd e prelevare solo quello che serve
            rff_tot += calcola_rff(ds, N, d, f, i, media_elem, out);
    }
    input->rff[f] = rff_tot;
    return (type) (((type) (dim+1) * (somma_rcf(rcf,dim,out)+rcf[f])/(type)(dim+1) / (sqrtf((type) (dim+1) + ((type) (dim+1) * ((type) (dim+1) - 1) *
                                                                                                                            (rff_tot + rff_totale) /
                                                                                                                         (type) (dim+coppie))))));
}
int checkout(const int* out,int dim, int f){
    for(int i=0; i< dim; i++){
        if(out[i]==f) return  1;
    }
    return 0;
}

void cfs(params *input, type media_elem[]) {
    // ------------------------------------------------------------
    // Codificare qui l'algoritmo di Correlation Features Selection
    // ------------------------------------------------------------
    int coppie=0;
    type merit_attuale;
    type merit_max = 0;
    type merit_massimissimo = 0;
    int f_merit_max = 0;
    type *rcf;


    int d = input->d;
    int k = input->k;
    type *ds = input->ds;
    int N = input->N;

    input->rff=alloc_matrix(N,d);

    rcf = calcola_max_rcf(input, ds, d, N);

    while (input->dim < k) {
        int *out = input->out;
        type rff_totale = input->rff_totale;
        for (int i = 0; i < d; i++) { //per ogni feature f (in questo caso d)
            if(checkout(out,input->dim,i)==0){
                merit_attuale = calcola_merit(input, ds, N, d, i, media_elem, rcf, out, rff_totale,
                                              input->dim,coppie); //passo la feature i-esima
                if (merit_attuale > merit_max) {
                    merit_max = merit_attuale;
                    f_merit_max = i;
                    if (merit_max > merit_massimissimo) {
                        merit_massimissimo = merit_max;
                    }
                }
            }
        }

        input->rff_totale += input->rff[f_merit_max];

        coppie+=input->dim;

        input->out[input->dim] = f_merit_max;
        input->dim++;

        merit_max = 0;
    }
    input->sc = merit_massimissimo;
}

int main(int argc, char **argv) {
    char fname[256];
    char *dsfilename = NULL;
    char *labelsfilename = NULL;
    clock_t t;
    float time;

    //
    // Imposta i valori di default dei parametri
    //

    params *input = malloc(sizeof(params));

    input->ds = NULL;
    input->labels = NULL;
    input->k = -1;
    input->sc = -1;

    input->silent = 0;
    input->display = 0;

    //
    // Visualizza la sintassi del passaggio dei parametri da riga di comando
    //

    if (argc <= 1) {
        printf("%s -ds <DS> -labels <LABELS> -k <K> [-s] [-d]\n", argv[0]);
        printf("\nParameters:\n");
        printf("\tDS: il nome del file ds2 contenente il dataset\n");
        printf("\tLABELS: il nome del file ds2 contenente le etichette\n");
        printf("\tk: numero di features da estrarre\n");
        printf("\nOptions:\n");
        printf("\t-s: modo silenzioso, nessuna stampa, default 0 - false\n");
        printf("\t-d: stampa a video i risultati, default 0 - false\n");
        exit(0);
    }

    //
    // Legge i valori dei parametri da riga comandi
    //

    int par = 1;
    while (par < argc) {
        if (strcmp(argv[par], "-s") == 0) {
            input->silent = 1;
            par++;
        } else if (strcmp(argv[par], "-d") == 0) {
            input->display = 1;
            par++;
        } else if (strcmp(argv[par], "-ds") == 0) {
            par++;
            if (par >= argc) {
                printf("Missing dataset file name!\n");
                exit(1);
            }
            dsfilename = argv[par];
            par++;
        } else if (strcmp(argv[par], "-labels") == 0) {
            par++;
            if (par >= argc) {
                printf("Missing labels file name!\n");
                exit(1);
            }
            labelsfilename = argv[par];
            par++;
        } else if (strcmp(argv[par], "-k") == 0) {
            par++;
            if (par >= argc) {
                printf("Missing k value!\n");
                exit(1);
            }
            input->k = atoi(argv[par]);
            par++;
        } else {
            printf("WARNING: unrecognized parameter '%s'!\n", argv[par]);
            par++;
        }
    }

    //
    // Legge i dati e verifica la correttezza dei parametri
    //

    if (dsfilename == NULL || strlen(dsfilename) == 0) {
        printf("Missing ds file name!\n");
        exit(1);
    }

    if (labelsfilename == NULL || strlen(labelsfilename) == 0) {
        printf("Missing labels file name!\n");
        exit(1);
    }


    input->ds = load_data(dsfilename, &input->N, &input->d);

    int nl, dl;
    input->labels = load_data(labelsfilename, &nl, &dl);

    if (nl != input->N || dl != 1) {
        printf("Invalid size of labels file, should be %ix1!\n", input->N);
        exit(1);
    }

    if (input->k <= 0) {
        printf("Invalid value of k parameter!\n");
        exit(1);
    }

    input->out = alloc_int_matrix(input->k, 1);

    //
    // Visualizza il valore dei parametri
    //

    if (!input->silent) {
        printf("Dataset file name: '%s'\n", dsfilename);
        printf("Labels file name: '%s'\n", labelsfilename);
        printf("Dataset row number: %d\n", input->N);
        printf("Dataset column number: %d\n", input->d);
        printf("Number of features to extract: %d\n", input->k);
    }


    //
    // Correlation Features Selection
    //
    int d = input->d;
    type *ds = input->ds;
    int N = input->N;
    type media_elem[d]; //Media degli elementi di ogni feature
    for (int i = 0; i < d; i++) {
        media_elem[i] = calcola_media(ds, N, d, i);
    }

    t = clock();
    cfs(input, media_elem);
    t = clock() - t;
    time = ((float) t) / CLOCKS_PER_SEC;

    if (!input->silent)
        printf("CFS time = %.3f secs\n", time);
    else
        printf("%.3f\n", time);

    //
    // Salva il risultato
    //
    sprintf(fname, "out32_%d_%d_%d.ds2", input->N, input->d, input->k);
    save_out(fname, input->sc, input->out, input->k);
    if (input->display) {
        if (input->out == NULL)
            printf("out: NULL\n");
        else {
            int i;
            printf("sc: %f, out: [", input->sc);
            for (i = 0; i < input->k; i++) {
                printf("%i,", input->out[i]);
            }
            printf("]\n");
        }
    }

    if (!input->silent)
        printf("\nDone.\n");

    dealloc_matrix(input->ds);
    dealloc_matrix(input->labels);
    dealloc_matrix(input->out);
    free(input);

    return 0;
}

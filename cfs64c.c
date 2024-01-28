/**************************************************************************************
* 
* CdL Magistrale in Ingegneria Informatica
* Corso di Architetture e Programmazione dei Sistemi di Elaborazione - a.a. 2020/21
* 
* Progetto dell'algoritmo Attention Mechanism 221 231 a
* in linguaggio assembly x86-64 + SSE
*
* Fabrizio Angiulli, aprile 2019
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
*    sudo apt-get install lib64gcc-4.8-dev (o altra versione)
*    sudo apt-get install libc6-dev-i386
*
* Per generare il file eseguibile:
*
* nasm -f elf64 fss64.nasm && gcc -m64 -msse -O0 -no-pie sseutils64.o fss64.o fss64c.c -o fss64c -lm && ./fss64c $pars
*
* oppure
*
* ./runfss64
*
*/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <libgen.h>
#include <xmmintrin.h>

#define	type		double
#define	MATRIX		type*
#define	VECTOR		type*

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
*
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

void* get_block(int size, int elements) {
	return _mm_malloc(elements*size,32);
}

void free_block(void* p) {
	_mm_free(p);
}

MATRIX alloc_matrix(int rows, int cols) {
	return (MATRIX) get_block(sizeof(type),rows*cols);
}

int* alloc_int_matrix(int rows, int cols) {
	return (int*) get_block(sizeof(int),rows*cols);
}

void dealloc_matrix(void* mat) {
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
* 	primi 4 byte: numero di colonne (N) --> numero intero
* 	successivi 4 byte: numero di righe (M) --> numero intero
* 	successivi N*M*8 byte: matrix data in row-major order --> numeri floating-point a precisione doppia
*
*****************************************************************************
*	Se lo si ritiene opportuno, � possibile cambiare la codifica in memoria
* 	della matrice.
*****************************************************************************
*
*/
MATRIX load_data(char* filename, int *n, int *k) {
    FILE *fp = fopen(filename, "rb"); //apre il file in lettura binaria
    int rows, cols;

    if (fp == NULL) {
        printf("'%s': nome del file errato!\n", filename);
        exit(0);
    }

    fread(&cols, sizeof(int), 1, fp);

    fread(&rows, sizeof(int), 1, fp);

    MATRIX data = alloc_matrix(rows, cols);

    type *temp = malloc(rows*cols*sizeof (type));
    fread(temp,sizeof (type),rows*cols,fp);

    for (int col=0; col < cols; col++){
        for (int row = 0; row < rows; row++) {
            data[col * rows + row]=temp[row * cols + col];
        }
    }
    free(temp);

    fclose(fp);

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
* 	primi 4 byte: numero di colonne (N) --> numero intero a 32 bit
* 	successivi 4 byte: numero di righe (M) --> numero intero a 32 bit
* 	successivi N*M*8 byte: matrix data in row-major order --> numeri interi o floating-point a precisione doppia
*/
void save_data(char* filename, void* X, int n, int k) {
	FILE* fp;
	int i;
	fp = fopen(filename, "wb");
	if(X != NULL){
		fwrite(&k, 4, 1, fp);
		fwrite(&n, 4, 1, fp);
		for (i = 0; i < n; i++) {
			fwrite(X, sizeof(type), k, fp);
			//printf("%i %i\n", ((int*)X)[0], ((int*)X)[1]);
			X += sizeof(type)*k;
		}
	}
	else{
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
* 	primi 4 byte: contenenti il numero di elementi (k+1)		--> numero intero a 32 bit
* 	successivi 4 byte: numero di righe (1) 						--> numero intero a 32 bit
* 	successivi byte: elementi del vettore 		--> 1 numero floating-point a precisione doppia e k interi
*/
void save_out(char* filename, type sc, int* X, int k) {
	FILE* fp;
	int n = 1;
	k++;
	fp = fopen(filename, "wb");
	if(X != NULL){
		fwrite(&n, 4, 1, fp);
		fwrite(&k, 4, 1, fp);
		fwrite(&sc, sizeof(type), 1, fp);
		fwrite(X, sizeof(int), k, fp);
		//printf("%i %i\n", ((int*)X)[0], ((int*)X)[1]);
	}
	fclose(fp);
}

// PROCEDURE ASSEMBLY

extern void prova(params* input);


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
type calcola_media(const type *ds, int N, int f) {
    type somma = 0;
    int inizio = f * N;
    for (int i = 0; i < N; i ++) {
        somma += ds[inizio + i];
    }
    return somma / (type) (N);
}

//GIUSTO
type deviazione_standard_c(const type *ds, int N, int f) {
    type media = calcola_media(ds, N, f);
    type somma_quadrati_diff = 0;
    int inizio = f*N;

    for (int i = 0; i < N; i++) {
        type diff = ds[inizio + i] - media;
        somma_quadrati_diff += diff * diff;
    }
    return sqrtf(somma_quadrati_diff / (type) (N-1));
}
// Metodo per precalcolare i valori medi per ogni feature
void precalcola_rcf(const type *ds, const type *labels, int N, int d, type *rcf_precalcolati) {
    for (int f = 0; f < d; f++) {
        type sum_0 = 0;
        int count_0 = 0;
        type sum_1 = 0;
        int count_1 = 0;

        for (int i = 0; i < N; i++) {
            if (labels[i] == 0) {
                sum_0 += ds[f * N + i];
                count_0++;
            } else if (labels[i] == 1) {
                sum_1 += ds[f * N + i];
                count_1++;
            }
        }

        rcf_precalcolati[f * 2] = count_0 > 0 ? sum_0 / (type)count_0 : 0; // Media per la classe 0
        rcf_precalcolati[f * 2 + 1] = count_1 > 0 ? sum_1 / (type)count_1 : 0; // Media per la classe 1
    }
}

//GIUSTO
type calcola_rcf(const type *ds, int N, int f, int n0, int n1, const type* rcf_precalcolati) {
    type mu0 = rcf_precalcolati[f * 2];     // Utilizzando il valore medio per la classe 0
    type mu1 = rcf_precalcolati[f * 2 + 1]; // Utilizzando il valore medio per la classe 1

    type sf = deviazione_standard_c(ds, N, f);

    int n = n0 + n1;
    return fabs(((mu0 - mu1) / sf) * sqrtf((type)(n0 * n1) / (type)(n * n)));
}

//GIUSTO
type* calcola_max_rcf(params *input, const type *ds, int d, int N, type* rcf_precalcolati) {
    int f_rcf_max = 0;
    type rcf_max = 0;
    type *label = input->labels;

    type *rcf = (type *) malloc(d * sizeof(type));

    int n1 = conta_elementi_1(label, N);
    int n0 = N - n1;

    for (int i = 0; i < d; i++) {
        rcf[i] = calcola_rcf(ds, N, i, n0, n1, rcf_precalcolati);

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
/*
type calcola_rff(const type *ds, int N, int fx, int fy, type media_elem_fx, type media_elem_fy) {
    type mux = media_elem_fx;
    type muy = media_elem_fy;
    type sommatoria1 = 0;
    type sommatoria2 = 0;
    type sommatoria3 = 0;
    int inizio_fx = fx*N;
    int inizio_fy = fy*N;


    for (int i = 0; i < N; i ++) {
       type operazione1 = ds[inizio_fx + i] - mux;
       type operazione2 = ds[inizio_fy+ i] - muy;
       sommatoria1 += ((operazione1) * (operazione2));
       sommatoria2 += ((operazione1) * (operazione1));
       sommatoria3 += ((operazione2) * (operazione2));
    }
    return fabsf((type) (sommatoria1 / (sqrtf(sommatoria2 * sommatoria3))));
}*/
extern type calcola_rff_new(const type *ds, int N, int fx, int fy, type media_elem_fx, type media_elem_fy, type* ret);


type somma_rcf(const type *rcf, int dim, const int* out) {
    type rcf_tot = 0;
    for (int i = 0; i < dim; i++) {
        rcf_tot += rcf[out[i]];
    }
    return rcf_tot;
}

type calcola_merit(params *input, const type *ds, int N, int f, type media_elem[], type *rcf, int *out,
                   type rff_totale, int dim, int coppie) {
    type rff_tot = 0;
    type x=0;
    for (int i = 0; i < dim; i++) {
        //rff_tot += fabsf(calcola_rff_new(ds, N, d, f, out[i], media_elem[f],media_elem[out[i]]));
        //printf("RFF tra %d e %d è: %f\n",f,out[i], fabsf(calcola_rff_new(ds, N, d, f, out[i], media_elem[f],media_elem[out[i]])));
        calcola_rff_new(ds, N, f, out[i], media_elem[f],media_elem[out[i]] ,&x);
        //printf("RFF %d - %d: %f\n",f,out[i],fabs(x));
        rff_tot+=fabs(x);
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
    type* labels = input->labels;

    type* rcf_precalcolati = (type *)malloc(2 * d * sizeof(type));
    if (rcf_precalcolati == NULL) {
        fprintf(stderr, "Errore di allocazione memoria per rcf_precalcolati\n");
        exit(1);
    }

    // Calcola i valori medi precalcolati
    precalcola_rcf(ds, labels, N, d, rcf_precalcolati);

    input->rff=alloc_matrix(N,d);

    rcf = calcola_max_rcf(input, ds, d, N, rcf_precalcolati);

    while (input->dim < k) {
        int *out = input->out;
        type rff_totale = input->rff_totale;
        for (int i = 0; i < d; i++) { //per ogni feature f (in questo caso d)
            if(checkout(out,input->dim,i)==0){
                merit_attuale = calcola_merit(input, ds, N, i, media_elem, rcf, out, rff_totale,
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

int main(int argc, char** argv) {

	char fname[256];
	char* dsfilename = NULL;
	char* labelsfilename = NULL;
	clock_t t;
	float time;

	//
	// Imposta i valori di default dei parametri
	//

	params* input = malloc(sizeof(params));

	input->ds = NULL;
	input->labels = NULL;
	input->k = -1;
	input->sc = -1;

	input->silent = 0;
	input->display = 0;

	printf("%i\n", sizeof(int));

	//
	// Visualizza la sintassi del passaggio dei parametri da riga comandi
	//

	if(argc <= 1){
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
		if (strcmp(argv[par],"-s") == 0) {
			input->silent = 1;
			par++;
		} else if (strcmp(argv[par],"-d") == 0) {
			input->display = 1;
			par++;
		} else if (strcmp(argv[par],"-ds") == 0) {
			par++;
			if (par >= argc) {
				printf("Missing dataset file name!\n");
				exit(1);
			}
			dsfilename = argv[par];
			par++;
		} else if (strcmp(argv[par],"-labels") == 0) {
			par++;
			if (par >= argc) {
				printf("Missing labels file name!\n");
				exit(1);
			}
			labelsfilename = argv[par];
			par++;
		} else if (strcmp(argv[par],"-k") == 0) {
			par++;
			if (par >= argc) {
				printf("Missing k value!\n");
				exit(1);
			}
			input->k = atoi(argv[par]);
			par++;
		} else{
			printf("WARNING: unrecognized parameter '%s'!\n",argv[par]);
			par++;
		}
	}

	//
	// Legge i dati e verifica la correttezza dei parametri
	//

	if(dsfilename == NULL || strlen(dsfilename) == 0){
		printf("Missing ds file name!\n");
		exit(1);
	}

	if(labelsfilename == NULL || strlen(labelsfilename) == 0){
		printf("Missing labels file name!\n");
		exit(1);
	}


	input->ds = load_data(dsfilename, &input->N, &input->d);

	int nl, dl;
	input->labels = load_data(labelsfilename, &nl, &dl);

	if(nl != input->N || dl != 1){
		printf("Invalid size of labels file, should be %ix1!\n", input->N);
		exit(1);
	}

	if(input->k <= 0){
		printf("Invalid value of k parameter!\n");
		exit(1);
	}

	input->out = alloc_int_matrix(input->k, 1);

	//
	// Visualizza il valore dei parametri
	//

	if(!input->silent){
		printf("Dataset file name: '%s'\n", dsfilename);
		printf("Labels file name: '%s'\n", labelsfilename);
		printf("Dataset row number: %d\n", input->N);
		printf("Dataset column number: %d\n", input->d);
		printf("Number of features to extract: %d\n", input->k);
	}

	// COMMENTARE QUESTA RIGA!
	//prova(input);
	//

	//
	// Correlation Features Selection
	//
    int d = input->d;
    type *ds = input->ds;
    int N = input->N;
    type media_elem[d]; //Media degli elementi di ogni feature
    for (int i = 0; i < d; i++) {
        media_elem[i] = calcola_media(ds, N, i);
    }

	t = clock();
	cfs(input, media_elem);
	t = clock() - t;
	time = ((float)t)/CLOCKS_PER_SEC;

	if(!input->silent)
		printf("CFS time = %.3f secs\n", time);
	else
		printf("%.3f\n", time);

	//
	// Salva il risultato
	//
	sprintf(fname, "out64_%d_%d_%d.ds2", input->N, input->d, input->k);
	save_out(fname, input->sc, input->out, input->k);
	if(input->display){
		if(input->out == NULL)
			printf("out: NULL\n");
		else{
			int i,j;
			printf("sc: %lf, out: [", input->sc);
			for(i=0; i<input->k; i++){
				printf("%i,", input->out[i]);
			}
			printf("]\n");
		}
	}

	if(!input->silent)
		printf("\nDone.\n");

	dealloc_matrix(input->ds);
	dealloc_matrix(input->labels);
	dealloc_matrix(input->out);
	free(input);

	return 0;
}

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <libgen.h>
#include <xmmintrin.h>

//riserva un blocco di memoria
void* get_block(int size, int elements) {
    return _mm_malloc(elements*size,16);
    //è generalmente utilizzata per allocare memoria allineata
    //in modo specifico per migliorare le prestazioni delle istruzioni SIMD.
}

//libera il blocco di memoria
void free_block(void* p) {
    _mm_free(p);
}

float* alloc_matrix(int rows, int cols) {
    return (float *) get_block(sizeof(float),rows*cols);
}

int main(){
    FILE* fp;
    int rows, cols, status, i;

    fp = fopen("test_5000_50_32.ds", "rb"); //apre il file in lettura binaria

    if (fp == NULL){
        printf("'%s': nome del file errato!\n", "test_5000_50_32.ds");
        exit(0);
    }


    //leggono rispettivamente la dimensione della riga e della colonna della matrice mettendoli nelle apposite variabili
    status = fread(&cols, sizeof(int), 1, fp);
    //ptr: puntatore al blocco di memoria in cui verranno memorizzati i dati
    //size: dimensione in byte dei dati
    //n: numero di elementi da leggere
    //stream: puntatore al file da cui leggere i dati

    status = fread(&rows, sizeof(int), 1, fp);

    //alloca la matrice "data" con dimensione rows e cols
    float* data = alloc_matrix(rows,cols);

    //popola la matrice con gli elementi all'interno del file
    status = fread(data, sizeof(float), rows*cols, fp);
    fclose(fp);

    for(int i=0; i<1000; i++){
        printf("%f,",data[i]);
    }

    // Stampa degli elementi della matrice
    printf("\nMatrice inserita:\n");
    for (int i = 0; i < rows/70; i++) {
        for (int j = 0; j < cols; j++) {
            printf("%f\t", data[i * cols + j]);
        }
        printf("\n");
    }

}
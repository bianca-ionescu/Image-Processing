/* ----- Implementare seriala functionala ----- */

#include<mpi.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

typedef struct {
	unsigned char** matrix;
	int width, height;
	unsigned char maxVal;
	char type[3];
	char nr_culori;
} image;

void readInput(const char *fileName, image *img) {
	FILE* f = fopen(fileName, "rb");
	if (f == NULL) return; /* Can't open file for writing */

	fgets(img->type, 3, f);
	fscanf(f, "%d %d", &img->width, &img->height);
	fscanf(f, "%hhu ", &img->maxVal);

	if (img->type[1] == '5') 
		img->nr_culori = 1;
	else if (img->type[1] == '6')
		img->nr_culori = 3;

	/* allocating the matrix */
	img->matrix = malloc(img->height * sizeof(unsigned char*));

	int i;
	for(i = 0; i < img->height; i++) 
		img->matrix[i] = malloc(img->nr_culori * img->width * sizeof(unsigned char));

	/* reading the image */
	for(i = 0; i < img->height; i++)
		fread(img->matrix[i], img->nr_culori * sizeof(unsigned char), img->width, f);

	fclose(f);
}

void writeData(const char *fileName, image *img) {
	FILE* f = fopen(fileName, "wb");
	if (f == NULL) return; /* Can't open file for writing */

	fprintf(f, "%s\n%d %d\n", img->type, img->width, img->height);
	fprintf(f, "%d\n", img->maxVal);
	
	int i;
	for(i = 0; i < img->height; i++)
		fwrite(img->matrix[i], img->nr_culori * sizeof(char), img->width, f);
	
	fclose(f);
}

void setFilterMatrix(float *matrix, char *filter) {
	int i;
	
	if(strcmp(filter, "smooth") == 0) {
		for(i = 0; i < 9; i++)
			matrix[i] = 1.0 / 9;
	
	} else if(strcmp(filter, "blur") == 0) {
		for(i = 0; i < 9; i++){
			if(i == 4)
				matrix[i] = 4.0 / 16;
			else if(i % 2 == 0)
				matrix[i] = 1.0 / 16;
			else if(i % 2 == 1)
				matrix[i] = 2.0 / 16;
		}

	} else if(strcmp(filter, "sharpen") == 0) {
		for(i = 0; i < 9; i++){
			if(i == 4)
				matrix[i] = 11.0 / 3;
			else if(i % 2 == 0)
				matrix[i] = 0;
			else if(i % 2 == 1)
				matrix[i] = -2.0 / 3;
		}

	} else if(strcmp(filter, "mean") == 0) {
		for(i = 0; i < 9; i++) {
			if(i != 4)
				matrix[i] = -1;
			else
				matrix[i] = 9;
		}
			
	} else if(strcmp(filter, "emboss") == 0) {
		for(i = 0; i < 9; i++) {
			if(i == 1)
				matrix[i] = 1;
			else if(i == 7)
				matrix[i] = -1;
			else
				matrix[i] = 0;
		}

	} else { // identity
		for(i = 0; i < 9; i++) {
			if(i != 4)
				matrix[i] = 0;
			else
				matrix[i] = 1;
		}
	}
}

void applyFilter(image *in, image *out, char *filter) {
	int i, j;

	float *matrix = (float*) malloc(9 * sizeof(float));
	setFilterMatrix(matrix, filter);

	for(i = 0; i < in->height; i++)
		for(j = 0; j < in->nr_culori * in->width; j++) {
			if(/* --- margini black & white --- */
				i == 0 || j == 0 || 
				i == in->height - 1 || j == in->nr_culori * (in->width - 1) ||
				/* --- margini color --- */
				(in->nr_culori == 3 && 
				(j == 1 || j == 2 || 
				(j == in->nr_culori * (in->width - 1) + 1 || 
				(j == in->nr_culori * (in->width - 1) + 2))))) {
				
				out->matrix[i][j] = in->matrix[i][j];
			
			} else {
				out->matrix[i][j] = matrix[0] * in->matrix[i - 1][j - in->nr_culori] +
									matrix[1] * in->matrix[i - 1][j] +
									matrix[2] * in->matrix[i - 1][j + in->nr_culori] +
									matrix[3] * in->matrix[i][j - in->nr_culori] +
									matrix[4] * in->matrix[i][j] +
									matrix[5] * in->matrix[i][j + in->nr_culori] +
									matrix[6] * in->matrix[i + 1][j - in->nr_culori] +
									matrix[7] * in->matrix[i + 1][j] +
									matrix[8] * in->matrix[i + 1][j + in->nr_culori];
		}
	}
}

int main(int argc, char *argv[]) {
	int i, j;
	int rank;
	int nProcesses;

	MPI_Init(&argc, &argv);

	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &nProcesses);
	
	image *in = malloc(sizeof(image));
	image *out = malloc(sizeof(image));

	if(rank == 0) { 
		readInput(argv[1], in);
		
		/* ----- Initializare matrice output ----- */
		out->width = in->width;
		out->height = in->height;
		out->maxVal = in->maxVal;
		strcpy(out->type, in->type);
		out->nr_culori = in->nr_culori;

		out->matrix = malloc(out->height * sizeof(unsigned char*));
		for(i = 0; i < in->height; i++) 
			out->matrix[i] = malloc(out->width * out->nr_culori * sizeof(unsigned char));
		
		int filter_count;
		for(filter_count = 3; filter_count < argc; filter_count++) {
			applyFilter(in, out, argv[filter_count]);
			
			/* ----- Updatarea matricii de output ----- */
			for(i = 0; i < in->height; i++)
				for(j = 0; j < in->nr_culori * in->width; j++) 
					in->matrix[i][j] = out->matrix[i][j];
		}

		writeData(argv[2], out);
	}

	MPI_Finalize();
	return 0;
}
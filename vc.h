#define VC_DEBUG

//ESTRUTURA DE UMA IMAGEM  

typedef struct {
	unsigned char *data;
	int width, height;
	int channels;			// Bin�rio/Cinzentos=1; RGB=3
	int levels;				// Bin�rio=1; Cinzentos [1,255]; RGB [1,255]
	int bytesperline;		// width * channels
} IVC;

typedef struct {
	int x, y, width, height;	// Caixa Delimitadora (Bounding Box)
	int area;					// Área
	int xc, yc;					// Centro-de-massa
	int perimeter;				// Perímetro
	int label;					// Etiqueta
} OVC;

// Funções: alocar e libertar uma imagem
IVC *vc_image_new(int width, int height, int channels, int levels);
IVC *vc_image_free(IVC *image);

// Funções: LEITURA E ESCRITA DE IMAGENS (PBM, PGM E PPM)
//IVC *vc_read_image(char *filename);
//int vc_write_image(char *filename, IVC *image);

int vc_image_diff(IVC *current, IVC *prev, IVC *dst);
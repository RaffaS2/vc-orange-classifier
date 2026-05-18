#define VC_DEBUG

// Estrutura de uma Imagem
typedef struct {
	unsigned char *data;
	int width, height;
	int channels;			// Binário/Cinzentos=1; RGB=3
	int levels;				// Binário=1; Cinzentos [1,255]; RGB [1,255]
	int bytesperline;		// width * channels
} IVC;

// Estrutura de um Blob
typedef struct {
	int x, y, width, height;	// Caixa Delimitadora (Bounding Box)
	int area;					// Área
	int xc, yc;					// Centro-de-massa
	int perimeter;				// Perímetro
	int label;					// Etiqueta
} OVC;

// Alocar e libertar uma imagem	
IVC *vc_image_new(int width, int height, int channels, int levels);
IVC *vc_image_free(IVC *image);

// Converção de BGR para HSV
int vc_bgr_to_hsv(IVC *src, IVC *dst);

// Segmentação de uma imagem em HSV com base em intervalos de cor (thresholds)
int vc_hsv_segmentation(IVC *src, IVC *dst, int hmin, int hmax, int smin, int smax, int vmin, int vmax);

// Operadores morfológicos 
int vc_binary_erosion(IVC *src, IVC *dst, int kernel);
int vc_binary_dilation(IVC *src, IVC *dst, int kernel);

// Labelling
OVC* vc_binary_blob_labelling(IVC *src, IVC *dst, int *nlabels);
int vc_binary_blob_info(IVC *src, OVC *blobs, int nblobs);

// Validação das laranjas com base no Regulamento CEE 379/71
int vc_orange_calibre(OVC blob);
int vc_orange_is_valid(OVC blob, int width, int height);

// Funções: LEITURA E ESCRITA DE IMAGENS (PBM, PGM E PPM)
//IVC *vc_read_image(char *filename);
//int vc_write_image(char *filename, IVC *image);

//int vc_image_diff(IVC *current, IVC *prev, IVC *dst);


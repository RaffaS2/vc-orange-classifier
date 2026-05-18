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
    int x, y, width, height;
    int area;
    int xc, yc;
    int perimeter;
    int label;
    float defect_ratio;   
    int defect_measured;  // 0 = ainda não medido | 1 = valor estável, reutilizar
} OVC;

// Representa uma laranja rastreada entre frames
typedef struct {
    int xc, yc;    // centróide
    int active;    // 1 = visível na frame atual
} TrackedOrange;

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

int vc_count_entered_oranges(OVC *prev, int nprev, OVC *curr, int ncurr, float max_dist);

// Funções para calcular e classificar a categoria da laranja
float vc_orange_defect_ratio(OVC blob, IVC *hsv);
const char* vc_orange_category(float defect_ratio);


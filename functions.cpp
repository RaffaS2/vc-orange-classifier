#include <iostream>
#include <string>
#include <cstring>
#include <stdlib.h>
#include "functions.h"


IVC *vc_image_new(int width, int height, int channels, int levels)
{
	IVC *image = (IVC *) malloc(sizeof(IVC));

	if(image == NULL) return NULL; 
	if((levels <= 0) || (levels > 255)) return NULL;

	image->width = width;
	image->height = height;
	image->channels = channels;
	image->levels = levels;
	image->bytesperline = image->width * image->channels;
	image->data = (unsigned char *) malloc(image->width * image->height * image->channels * sizeof(char));

	if(image->data == NULL)
	{
		return vc_image_free(image);
	}

	return image;
}

// Libertar memória de uma imagem
IVC *vc_image_free(IVC *image)
{
	if(image != NULL)
	{
		if(image->data != NULL)
		{
			free(image->data);
			image->data = NULL;
		}

		free(image);
		image = NULL;
	}

	return image;
}

 /**
 * @brief Converte uma imagem BGR (formato do OpenCV!!!) para HSV.
 *
 * @details
 * O OpenCV armazena píxeis em BGR (não RGB). Para segmentar laranjas
 * por cor de forma robusta à iluminação, convertemos para HSV onde:
 *   - H (Hue)        -> cor pura, independente do brilho
 *   - S (Saturation) -> intensidade da cor
 *   - V (Value)      -> brilho
 *
 * Assim, uma laranja iluminada e uma na sombra partilham o mesmo H (tonalidade),
 * tornando a segmentação muito mais fiável do que em BGR.
 *
 * Mapeamento de saída para [0,255]:
 *   - H: [0°, 360°] -> [0, 255]
 *   - S: [0, 1]     -> [0, 255]
 *   - V: [0, 1]     ->[0, 255]
 *
 * @param src Imagem de entrada em BGR (3 canais, vinda do OpenCV).
 * @param dst Imagem de saída em HSV  (3 canais).
 * @return    1 em caso de sucesso, 0 em caso de erro.
 */
int vc_bgr_to_hsv(IVC *src, IVC *dst)
{
    unsigned char *datasrc = (unsigned char *)src->data;
    int bytesperline_src = src->width * src->channels;
    int channels_src = src->channels;
    unsigned char *datadst = (unsigned char *)dst->data;
    int bytesperline_dst = dst->width * dst->channels;
    int channels_dst = dst->channels;
    int width = src->width;
    int height = src->height;
    int x, y;
    long int pos_src, pos_dst;
    float r, g, b, max, min, delta;
    float h, s, v;

    if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL))
        return 0;
    if ((src->width != dst->width) || (src->height != dst->height))
        return 0;
    if ((src->channels != 3) || (dst->channels != 3))
        return 0;

    for (y = 0; y < height; y++)
    {
        for (x = 0; x < width; x++)
        {
            pos_src = y * bytesperline_src + x * channels_src;
            pos_dst = y * bytesperline_dst + x * channels_dst;

            // OpenCV armazena em BGR índices invertidos face ao RGB
            b = (float)datasrc[pos_src + 0] / 255.0f;
            g = (float)datasrc[pos_src + 1] / 255.0f;
            r = (float)datasrc[pos_src + 2] / 255.0f;

            max = r;
            if (g > max) max = g;
            if (b > max) max = b;

            min = r;
            if (g < min) min = g;
            if (b < min) min = b;

            delta = max - min;
            v = max;

            if (max == 0.0f)
                s = 0.0f;
            else
                s = delta / max;

            if (delta == 0.0f)
                h = 0.0f;
            else if (max == r)
                h = 60.0f * ((g - b) / delta);
            else if (max == g)
                h = 60.0f * (2.0f + (b - r) / delta);
            else
                h = 60.0f * (4.0f + (r - g) / delta);

            if (h < 0.0f) h += 360.0f;

            datadst[pos_dst]     = (unsigned char)((h / 360.0f) * 255.0f);
            datadst[pos_dst + 1] = (unsigned char)(s * 255.0f);
            datadst[pos_dst + 2] = (unsigned char)(v * 255.0f);
        }
    }

    return 1;
}

/**
 * @brief Segmenta uma imagem HSV produzindo uma máscara binária.
 *
 * @details
 * Percorre cada píxel da imagem HSV e verifica se os seus valores de
 * H, S e V se encontram dentro dos intervalos definidos. O resultado
 * é uma imagem binária onde:
 *   - 255 -> píxel pertence à cor segmentada (laranja)
 *   -   0 -> píxel pertence ao fundo
 *
 * O canal H é tratado como circular (0° = 360°), pelo que intervalos
 * que cruzem o 0° (ex: hmin=350, hmax=10) são suportados via OR.
 *
 *
 * @param src  Imagem de entrada em HSV (3 canais).
 * @param dst  Imagem de saída binária  (1 canal).
 * @param hmin Limite inferior de Hue        [0, 360] em graus.
 * @param hmax Limite superior de Hue        [0, 360] em graus.
 * @param smin Limite inferior de Saturation [0, 100] em percentagem.
 * @param smax Limite superior de Saturation [0, 100] em percentagem.
 * @param vmin Limite inferior de Value      [0, 100] em percentagem.
 * @param vmax Limite superior de Value      [0, 100] em percentagem.
 * @return 1 em caso de sucesso, 0 em caso de erro.
 */
int vc_hsv_segmentation(IVC *src, IVC *dst, int hmin, int hmax, int smin, int smax, int vmin, int vmax)
{
	unsigned char *datasrc = (unsigned char *)src->data;
    int bytesperline_src = src->width * src->channels;
    int channels_src = src->channels;
    unsigned char *datadst = (unsigned char *)dst->data;
    int bytesperline_dst = dst->width * dst->channels;
    int channels_dst = dst->channels;
    int width = src->width;
    int height = src->height;
    int x, y;
    long int pos_src, pos_dst;
    unsigned char h, s, v;
    int hmin_255, hmax_255, smin_255, smax_255, vmin_255, vmax_255;
    int in_h, in_s, in_v;

    // Verificação de erros
    if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL))
        return 0;
    if ((src->width != dst->width) || (src->height != dst->height))
        return 0;
    if ((src->channels != 3) || (dst->channels != 1))
        return 0;

    // Intervalos válidos: H [0,360], S/V [0,100]
    if ((hmin < 0) || (hmin > 360) || (hmax < 0) || (hmax > 360))
        return 0;
    if ((smin < 0) || (smin > 100) || (smax < 0) || (smax > 100) || (smin > smax))
        return 0;
    if ((vmin < 0) || (vmin > 100) || (vmax < 0) || (vmax > 100) || (vmin > vmax))
        return 0;

    hmin_255 = (hmin * 255) / 360;
    hmax_255 = (hmax * 255) / 360;
    smin_255 = (smin * 255) / 100;
    smax_255 = (smax * 255) / 100;
    vmin_255 = (vmin * 255) / 100;
    vmax_255 = (vmax * 255) / 100;

    for (y = 0; y < height; y++)
    {
        for (x = 0; x < width; x++)
        {
            pos_src = y * bytesperline_src + x * channels_src;
            pos_dst = y * bytesperline_dst + x * channels_dst;

            h = datasrc[pos_src];
            s = datasrc[pos_src + 1];
            v = datasrc[pos_src + 2];

            // H é circular: se hmin > hmax, o intervalo cruza 0º
            if (hmin_255 <= hmax_255)
                in_h = (h >= hmin_255 && h <= hmax_255);
            else
                in_h = (h >= hmin_255 || h <= hmax_255);

            in_s = (s >= smin_255 && s <= smax_255);
            in_v = (v >= vmin_255 && v <= vmax_255);

            datadst[pos_dst] = (in_h && in_s && in_v) ? 255 : 0;

            if (in_h && in_s && in_v){
                datadst[pos_dst] = 255;  // branco
            }else{
                datadst[pos_dst] = 0;    // preto
            }
        }
    }

    return 1;
}

/**
 * @brief Aplica erosão binária numa imagem.
 *
 * Remove píxeis brancos isolados e reduz objetos brancos.
 * Um píxel só fica branco se todos os vizinhos dentro do
 * kernel também forem brancos.
 *
 * @param src Imagem de entrada binária.
 * @param dst Imagem de saída binária.
 * @param kernel Raio do kernel quadrado.
 *
 * @return 1 em caso de sucesso, 0 em caso de erro.
 */
int vc_binary_erosion(IVC *src, IVC *dst, int kernel)
{
    unsigned char *datasrc = (unsigned char *)src->data;
    unsigned char *datadst = (unsigned char *)dst->data;
    int width    = src->width;
    int height   = src->height;
    int bpl      = src->bytesperline;
    int x, y, kx, ky;
    int all_white;
    long int pos_dst;

    if (src->channels != 1 || dst->channels != 1) return 0;
    if (src->width != dst->width || src->height != dst->height) return 0;

    for (y = 0; y < height; y++)
    {
        for (x = 0; x < width; x++)
        {
            pos_dst = y * bpl + x;
            all_white = 1;

            // Verifica todos os vizinhos dentro do kernel
            for (ky = -kernel; ky <= kernel && all_white; ky++)
            {
                for (kx = -kernel; kx <= kernel && all_white; kx++)
                {
                    int nx = x + kx;
                    int ny = y + ky;

                    // Fora dos limites → trata como preto (fundo)
                    if (nx < 0 || nx >= width || ny < 0 || ny >= height)
                        all_white = 0;
                    else if (datasrc[ny * bpl + nx] == 0)
                        all_white = 0;
                }
            }

            datadst[pos_dst] = all_white ? 255 : 0;
        }
    }
    return 1;
}

/**
 * @brief Aplica dilatação binária numa imagem.
 *
 * Expande os objetos brancos e preenche pequenos buracos.
 * Um píxel fica branco se pelo menos um vizinho dentro do
 * kernel for branco.
 *
 * @param src Imagem de entrada binária.
 * @param dst Imagem de saída binária.
 * @param kernel Raio do kernel quadrado.
 *
 * @return 1 em caso de sucesso, 0 em caso de erro.
 */
int vc_binary_dilation(IVC *src, IVC *dst, int kernel)
{
    unsigned char *datasrc = (unsigned char *)src->data;
    unsigned char *datadst = (unsigned char *)dst->data;
    int width    = src->width;
    int height   = src->height;
    int bpl      = src->bytesperline;
    int x, y, kx, ky;
    int any_white;
    long int pos_dst;

    if (src->channels != 1 || dst->channels != 1) return 0;
    if (src->width != dst->width || src->height != dst->height) return 0;

    for (y = 0; y < height; y++)
    {
        for (x = 0; x < width; x++)
        {
            pos_dst = y * bpl + x;
            any_white = 0;

            // Fica branco se qualquer vizinho for branco
            for (ky = -kernel; ky <= kernel && !any_white; ky++)
            {
                for (kx = -kernel; kx <= kernel && !any_white; kx++)
                {
                    int nx = x + kx;
                    int ny = y + ky;

                    if (nx >= 0 && nx < width && ny >= 0 && ny < height)
                        if (datasrc[ny * bpl + nx] == 255)
                            any_white = 1;
                }
            }

            datadst[pos_dst] = any_white ? 255 : 0;
        }
    }
    return 1;
}

/**
 * @brief Realiza a etiquetagem de blobs numa imagem binária.
 *
 * Processa uma imagem binária de entrada e identifica regiões conectadas (blobs),
 * atribuindo-lhes etiquetas únicas numa imagem de saída em escala de cinzentos.
 *
 * @param src Imagem binária de entrada.
 * @param dst Imagem em escala de cinzentos onde serão armazenadas as etiquetas.
 * @param nlabels Apontador para uma variável inteira onde será armazenado o número de blobs encontrados.
 *
 * @return Array dinâmico de blobs (OVC). Deve ser libertado pelo utilizador.
 */
OVC* vc_binary_blob_labelling(IVC *src, IVC *dst, int *nlabels)
{
	unsigned char *datasrc = (unsigned char *)src->data;
	unsigned char *datadst = (unsigned char *)dst->data;
	int width = src->width;
	int height = src->height;
	int bytesperline = src->bytesperline;
	int channels = src->channels;
	int x, y, a, b;
	long int i, size;
	long int posX, posA, posB, posC, posD;
	int labeltable[256] = { 0 };
	int labelarea[256] = { 0 };
	int label = 1; // Etiqueta inicial.
	int num, tmplabel;
	OVC *blobs; // Apontador para array de blobs (objectos) que será retornado desta função.

	// Verificaçãoo de erros
	if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL)) return 0;
	if ((src->width != dst->width) || (src->height != dst->height) || (src->channels != dst->channels)) return NULL;
	if (channels != 1) return NULL;

	// Copia dados da imagem binária para imagem grayscale
	memcpy(datadst, datasrc, bytesperline * height);

	// Todos os pixeis de plano de fundo devem obrigatóriamente ter valor 0
	// Todos os pixeis de primeiro plano devem obrigatóriamente ter valor 255
	// Serão atribuídas etiquetas no intervalo [1,254]
	// Este algoritmo está assim limitado a 254 labels
	for (i = 0, size = bytesperline * height; i<size; i++)
	{
		if (datadst[i] != 0) datadst[i] = 255;
	}

	// Limpa os rebordos da imagem binária
	for (y = 0; y<height; y++)
	{
		datadst[y * bytesperline + 0 * channels] = 0;
		datadst[y * bytesperline + (width - 1) * channels] = 0;
	}
	for (x = 0; x<width; x++)
	{
		datadst[0 * bytesperline + x * channels] = 0;
		datadst[(height - 1) * bytesperline + x * channels] = 0;
	}

	// Efectua a etiquetagem
	for (y = 1; y<height - 1; y++)
	{
		for (x = 1; x<width - 1; x++)
		{
			// Kernel:
			// A B C
			// D X

			posA = (y - 1) * bytesperline + (x - 1) * channels; // A
			posB = (y - 1) * bytesperline + x * channels; // B
			posC = (y - 1) * bytesperline + (x + 1) * channels; // C
			posD = y * bytesperline + (x - 1) * channels; // D
			posX = y * bytesperline + x * channels; // X

			// Se o pixel foi marcado
			if (datadst[posX] != 0)
			{
				if ((datadst[posA] == 0) && (datadst[posB] == 0) && (datadst[posC] == 0) && (datadst[posD] == 0))
				{
					datadst[posX] = label;
					labeltable[label] = label;
					label++;
				}
				else
				{
					num = 255;

					// Se A está marcado
					if (datadst[posA] != 0) num = labeltable[datadst[posA]];
					// Se B está marcado, e é menor que a etiqueta "num"
					if ((datadst[posB] != 0) && (labeltable[datadst[posB]] < num)) num = labeltable[datadst[posB]];
					// Se C está marcado, e é menor que a etiqueta "num"
					if ((datadst[posC] != 0) && (labeltable[datadst[posC]] < num)) num = labeltable[datadst[posC]];
					// Se D está marcado, e é menor que a etiqueta "num"
					if ((datadst[posD] != 0) && (labeltable[datadst[posD]] < num)) num = labeltable[datadst[posD]];

					// Atribui a etiqueta ao pixel
					datadst[posX] = num;
					labeltable[num] = num;

					// Actualiza a tabela de etiquetas
					if (datadst[posA] != 0)
					{
						if (labeltable[datadst[posA]] != num)
						{
							for (tmplabel = labeltable[datadst[posA]], a = 1; a<label; a++)
							{
								if (labeltable[a] == tmplabel)
								{
									labeltable[a] = num;
								}
							}
						}
					}
					if (datadst[posB] != 0)
					{
						if (labeltable[datadst[posB]] != num)
						{
							for (tmplabel = labeltable[datadst[posB]], a = 1; a<label; a++)
							{
								if (labeltable[a] == tmplabel)
								{
									labeltable[a] = num;
								}
							}
						}
					}
					if (datadst[posC] != 0)
					{
						if (labeltable[datadst[posC]] != num)
						{
							for (tmplabel = labeltable[datadst[posC]], a = 1; a<label; a++)
							{
								if (labeltable[a] == tmplabel)
								{
									labeltable[a] = num;
								}
							}
						}
					}
					if (datadst[posD] != 0)
					{
						if (labeltable[datadst[posD]] != num)
						{
							for (tmplabel = labeltable[datadst[posD]], a = 1; a<label; a++)
							{
								if (labeltable[a] == tmplabel)
								{
									labeltable[a] = num;
								}
							}
						}
					}
				}
			}
		}
	}

	// Volta a etiquetar a imagem
	for (y = 1; y<height - 1; y++)
	{
		for (x = 1; x<width - 1; x++)
		{
			posX = y * bytesperline + x * channels; // X

			if (datadst[posX] != 0)
			{
				datadst[posX] = labeltable[datadst[posX]];
			}
		}
	}

	//printf("\nMax Label = %d\n", label);

	// Contagem do número de blobs
	// Passo 1: Eliminar, da tabela, etiquetas repetidas
	for (a = 1; a<label - 1; a++)
	{
		for (b = a + 1; b<label; b++)
		{
			if (labeltable[a] == labeltable[b]) labeltable[b] = 0;
		}
	}
	// Passo 2: Conta etiquetas e organiza a tabela de etiquetas, para que não hajam valores vazios (zero) entre etiquetas
	*nlabels = 0;
	for (a = 1; a<label; a++)
	{
		if (labeltable[a] != 0)
		{
			labeltable[*nlabels] = labeltable[a]; // Organiza tabela de etiquetas
			(*nlabels)++; // Conta etiquetas
		}
	}

	// Se não há blobs
	if (*nlabels == 0) return NULL;

	// Cria lista de blobs (objectos) e preenche a etiqueta
	blobs = (OVC *)calloc((*nlabels), sizeof(OVC));
	if (blobs != NULL)
	{
		for (a = 0; a<(*nlabels); a++) blobs[a].label = labeltable[a];
	}
	else return NULL;

	return blobs;
}

/**
 * @brief Calcula informação geométrica de blobs numa imagem etiquetada.
 *
 * Percorre uma imagem etiquetada e calcula propriedades como área,
 * perímetro, bounding box e centro de gravidade para cada blob.
 *
 * @param src Imagem de entrada com blobs etiquetados.
 * @param blobs Array de estruturas OVC já com as labels atribuídas.
 * @param nblobs Número de blobs a processar.
 *
 * @return 1 em caso de sucesso, 0 em caso de erro.
 */
int vc_binary_blob_info(IVC *src, OVC *blobs, int nblobs);
int vc_binary_blob_info(IVC *src, OVC *blobs, int nblobs)
{
	unsigned char *data = (unsigned char *)src->data;
	int width = src->width;
	int height = src->height;
	int bytesperline = src->bytesperline;
	int channels = src->channels;
	int x, y, i;
	long int pos;
	int xmin, ymin, xmax, ymax;
	long int sumx, sumy;

	// Verificação de erros
	if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL)) return 0;
	if (channels != 1) return 0;

	// Conta área de cada blob
	for (i = 0; i<nblobs; i++)
	{
		xmin = width - 1;
		ymin = height - 1;
		xmax = 0;
		ymax = 0;

		sumx = 0;
		sumy = 0;

		blobs[i].area = 0;

		for (y = 1; y<height - 1; y++)
		{
			for (x = 1; x<width - 1; x++)
			{
				pos = y * bytesperline + x * channels;

				if (data[pos] == blobs[i].label)
				{
					// Área
					blobs[i].area++;

					// Centro de Gravidade
					sumx += x;
					sumy += y;

					// Bounding Box
					if (xmin > x) xmin = x;
					if (ymin > y) ymin = y;
					if (xmax < x) xmax = x;
					if (ymax < y) ymax = y;

					// Perímetro
					// Se pelo menos um dos quatro vizinhos não pertence ao mesmo label, então é um pixel de contorno
					if ((data[pos - 1] != blobs[i].label) || (data[pos + 1] != blobs[i].label) || (data[pos - bytesperline] != blobs[i].label) || (data[pos + bytesperline] != blobs[i].label))
					{
						blobs[i].perimeter++;
					}
				}
			}
		}

		// Bounding Box
		blobs[i].x = xmin;
		blobs[i].y = ymin;
		blobs[i].width = (xmax - xmin) + 1;
		blobs[i].height = (ymax - ymin) + 1;

		// Centro de Gravidade
		//blobs[i].xc = (xmax - xmin) / 2;
		//blobs[i].yc = (ymax - ymin) / 2;
		blobs[i].xc = sumx / std::max(blobs[i].area, 1);
		blobs[i].yc = sumy / std::max(blobs[i].area, 1);
	}

	return 1;
}

// Funções de validação das laranjas com base no Regulamento CEE 379/71
// A escala do vídeo é 280px = 55mm -> 1px = 0.19643mm

 /**
 * @brief Calcula o calibre de uma laranja segundo o Regulamento CEE 379/71.
 *
 * O regulamento define tamanhos de 0 a 13 com base no diâmetro
 * equatorial (máximo entre largura e altura da bounding box).
 *
 * @param blob Estrutura que representa o objeto detetado (largura e altura em píxeis).
 * @return Calibre da laranja (0 a 13).
 * Retorna -1 se o objeto não for uma laranja válida (abaixo do mínimo de 53 mm).
 */
int vc_orange_calibre(OVC blob)
{
    float px_to_mm = 55.0f / 280.0f; // 1 px = 0.19643 mm
    int diameter_px = std::max(blob.width, blob.height);
    float diameter_mm = diameter_px * px_to_mm;

    if (diameter_mm >= 100) return 0;
    if (diameter_mm >= 87)  return 1;
    if (diameter_mm >= 84)  return 2;
    if (diameter_mm >= 81)  return 3;
    if (diameter_mm >= 77)  return 4;
    if (diameter_mm >= 73)  return 5;
    if (diameter_mm >= 70)  return 6; 
    if (diameter_mm >= 67)  return 7;
    if (diameter_mm >= 64)  return 8;
    if (diameter_mm >= 62)  return 9;
    if (diameter_mm >= 60)  return 10;
    if (diameter_mm >= 58)  return 11;
    if (diameter_mm >= 56)  return 12;
    if (diameter_mm >= 53)  return 13;

    return -1; // abaixo do mínimo (53mm) - não é uma laranja válida
}

/**
 * @brief Calcula e valida o tamanho de uma laranja a ser analisada.
 *
 * Verifica se um blob tem tamanho mínimo para ser considerado laranja.
 * O regulamento define 53 mm como diâmetro mínimo.
 * Abaixo disso considera-se ruído ou objeto inválido.
 *
 * @param blob Estrutura que representa o objeto detetado.
 * @return 1 se for uma laranja válida, 0 caso contrário.
 */
int vc_orange_is_valid(OVC blob)    
{
    float px_to_mm = 55.0f / 280.0f;
    int diameter_px = std::max(blob.width, blob.height);
    float diameter_mm = diameter_px * px_to_mm;

    return diameter_mm >= 53.0f; // 53mm = mínimo do regulamento
}
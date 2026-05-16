#include <iostream>
#include <string>
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
 * Valores sugeridos para laranjas:
 *   - H: [5°,  35°]
 *   - S: [45%, 100%]
 *   - V: [30%, 100%]
 *
 * @param src  Imagem de entrada em HSV (3 canais).
 * @param dst  Imagem de saída binária  (1 canal).
 * @param hmin Limite inferior de Hue        [0, 360] em graus.
 * @param hmax Limite superior de Hue        [0, 360] em graus.
 * @param smin Limite inferior de Saturation [0, 100] em percentagem.
 * @param smax Limite superior de Saturation [0, 100] em percentagem.
 * @param vmin Limite inferior de Value      [0, 100] em percentagem.
 * @param vmax Limite superior de Value      [0, 100] em percentagem.
 * @return     1 em caso de sucesso, 0 em caso de erro.
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
 * remove o ruído branco pequeno (aumenta zonas brancas)
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
 * (fecha os buracos pretos)
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
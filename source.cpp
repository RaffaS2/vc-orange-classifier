#include <iostream>
#include <string>
#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/videoio.hpp>

extern "C" {
#include "vc.h"

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

// Libertar mem�ria de uma imagem
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
int vc_image_diff(IVC *current, IVC *prev, IVC *diff){

	// Verificação de erros
    if(current == NULL || prev == NULL || diff == NULL) return 0;
	if(current->width  != prev->width  || current->width  != diff->width)  return 0;
    if(current->height != prev->height || current->height != diff->height) return 0;
    if(current->channels != prev->channels) return 0;
    
	int size = current->width * current->height * current->channels;

    for (int i = 0; i < size; i++) {
        // Diferença absoluta por canal
        int d = (int)current->data[i] - (int)prev->data[i];
        diff->data[i] = (unsigned char)abs(d);
    }

    return 1;
}

}

int main(void) {
	// V�deo
	char videofile[20] = "video.avi";
	cv::VideoCapture capture;
	struct
	{
		int width, height;
		int ntotalframes;
		int fps;
		int nframe;
	} video;
	// Outros
	std::string str;
	int key = 0;

	/* Leitura de v�deo de um ficheiro */
	/* NOTA IMPORTANTE:
	O ficheiro video.avi dever� estar localizado no mesmo direct�rio que o ficheiro de c�digo fonte.
	*/
	capture.open(videofile);
	
	/* Em alternativa, abrir captura de v�deo pela Webcam #0 */
	//capture.open(0, cv::CAP_DSHOW); // Pode-se utilizar apenas capture.open(0);
	
	/* Verifica se foi poss�vel abrir o ficheiro de v�deo */
	if (!capture.isOpened())
	{
		std::cerr << "Erro ao abrir o ficheiro de v�deo!\n";
		return 1;
	}	

	/* N�mero total de frames no v�deo */
	video.ntotalframes = (int)capture.get(cv::CAP_PROP_FRAME_COUNT);
	/* Frame rate do v�deo */
	video.fps = (int)capture.get(cv::CAP_PROP_FPS);
	/* Resolu��o do v�deo */
	video.width = (int)capture.get(cv::CAP_PROP_FRAME_WIDTH);
	video.height = (int)capture.get(cv::CAP_PROP_FRAME_HEIGHT);

	/* Cria uma janela para exibir o v�deo */
	cv::namedWindow("VC - VIDEO", cv::WINDOW_AUTOSIZE);
	
	
	IVC *imageCurrent = vc_image_new(video.width, video.height, 3, 255);
	IVC *imagePrev = vc_image_new(video.width, video.height, 3, 255);
	IVC *imageDiff = vc_image_new(video.width, video.height, 3, 255);

	cv::Mat frame;
while (key != 'q') {
    capture.read(frame);
    if (frame.empty()) break;

    // Copia frame atual → imageCurrent
    memcpy(imageCurrent->data, frame.data, video.width * video.height * 3);

    // Compara atual com anterior → resultado em imageDiff
    vc_image_diff(imageCurrent, imagePrev, imageDiff);

    // Copia resultado de volta para exibir com OpenCV
    memcpy(frame.data, imageDiff->data, video.width * video.height * 3);

    // Guarda atual como anterior para próxima iteração
    memcpy(imagePrev->data, imageCurrent->data, video.width * video.height * 3);

    cv::imshow("VC - VIDEO", frame);
    key = cv::waitKey(1);
}

vc_image_free(imageCurrent);
vc_image_free(imagePrev);
vc_image_free(imageDiff);
	/* Fecha a janela */
	cv::destroyWindow("VC - VIDEO");

	/* Fecha o ficheiro de v�deo */
	capture.release();

	return 0;
}
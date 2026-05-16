#include <iostream>
#include <string>
#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/videoio.hpp>
#include "functions.h"
//g++ source2.cpp -o programa $(pkg-config --cflags --libs opencv4)


int main(void)
{
    char videofile[20] = "video.avi";
    cv::VideoCapture capture;
    struct {
        int width, height;
        int ntotalframes;
        int fps;
        int nframe;
    } video;

    std::string str;
    int key = 0;

    // O ficheiro video.avi deverá estar localizado no mesmo directório que o ficheiro de código fonte.
    capture.open(videofile);
    if (!capture.isOpened()) {
        std::cerr << "Erro ao abrir o ficheiro de vídeo!\n";
        return 1;
    }

    // Número total de frames no vídeo 
    video.ntotalframes = (int)capture.get(cv::CAP_PROP_FRAME_COUNT);
    // Frame rate do vídeo 
    video.fps          = (int)capture.get(cv::CAP_PROP_FPS);
    // Resolução do vídeo 
    video.width        = (int)capture.get(cv::CAP_PROP_FRAME_WIDTH);
    video.height       = (int)capture.get(cv::CAP_PROP_FRAME_HEIGHT);

    cv::namedWindow("VC - ORIGINAL",   cv::WINDOW_AUTOSIZE);
    cv::namedWindow("VC - MASCARA", cv::WINDOW_AUTOSIZE); // debug

    // Aloca imagens IVC uma única vez, fora do loop 
    IVC *image_bgr  = vc_image_new(video.width, video.height, 3, 255);
    IVC *image_hsv  = vc_image_new(video.width, video.height, 3, 255);
    IVC *image_mask = vc_image_new(video.width, video.height, 1, 255);
    IVC *image_tmp  = vc_image_new(video.width, video.height, 1, 255);
    cv::Mat frame;
    while (key != 'q')
    {   
        // Leitura de uma frame do vídeo
        capture.read(frame);
        if (frame.empty()) break;

        /* Número da frame a processar */
        video.nframe = (int)capture.get(cv::CAP_PROP_POS_FRAMES);

        // Copiar o frame BGR do OpenCV para a estrutura IVC
        memcpy(image_bgr->data, frame.data, video.width * video.height * 3);

        // Converte BGR -> HSV (separa a cor do brilho)
        vc_bgr_to_hsv(image_bgr, image_hsv);

        // Segmentação: isola píxeis com cor de laranja
        // H[5°,35°]  S[45%,100%]  V[30%,100%]
        vc_hsv_segmentation(image_hsv, image_mask, 5, 35, 45, 100, 30, 100);
        //vc_hsv_segmentation(image_hsv, image_mask, 18, 30, 79, 100, 30, 100);

        vc_binary_erosion(image_mask, image_tmp,  3);
        vc_binary_dilation(image_tmp, image_mask, 3);   

        // Mostra a máscara binária (para debug)
        cv::Mat mask_mat(video.height, video.width, CV_8UC1, image_mask->data);
        cv::imshow("VC - MASCARA", mask_mat);

        // HUD — informação básica sobre o vídeo
        str = "Frame: " + std::to_string(video.nframe) + "/" +
              std::to_string(video.ntotalframes);
        cv::putText(frame, str, cv::Point(20, 30),
                    cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 0, 0), 2);
        cv::putText(frame, str, cv::Point(20, 30),
                    cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255, 255, 255), 1);

        cv::imshow("VC - ORIGINAL", frame);
        key = cv::waitKey(1000 / video.fps);
    }

    // libertar memória
    vc_image_free(image_bgr);
    vc_image_free(image_hsv);
    vc_image_free(image_mask);

    // Fecha a janela 
    cv::destroyAllWindows();
    //Fecha o ficheiro de vídeo 
    capture.release();
    return 0;
}
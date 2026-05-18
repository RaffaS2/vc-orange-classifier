#include <iostream>
#include <string>
#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/videoio.hpp>
#include <cstring>
#include <algorithm>
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
    int total_oranges = 0; 

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
    IVC *image_label  = vc_image_new(video.width, video.height, 1, 255);

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

        // não é a melhor segmentação mas faz desaparecer a maçã
        //vc_hsv_segmentation(image_hsv, image_mask, 18, 30, 79, 100, 30, 100);

        vc_binary_erosion(image_mask, image_tmp,  3);
        vc_binary_dilation(image_tmp, image_mask, 3);   

        // Etiquetagem: atribui um ID único a cada blob (laranja separada)
        int nlabels = 0;
        OVC *blobs = vc_binary_blob_labelling(image_mask, image_label, &nlabels);

        // Calcula métricas de cada blob (área, perímetro, bounding box, centróide)
        if (blobs != NULL)
            vc_binary_blob_info(image_label, blobs, nlabels);

        // Processa cada blob - desenha info e conta laranjas válidas na frame
        int frame_oranges = 0;
        if (blobs != NULL)
        {
            for (int i = 0; i < nlabels; i++)
            {   
                printf("Blob %d | area=%d | bbox=%dx%d | fill=%.2f\n",
                i, blobs[i].area, blobs[i].width, blobs[i].height,
                (float)blobs[i].area / (float)(blobs[i].width * blobs[i].height));

                // Ignora blobs abaixo do tamanho mínimo do regulamento (53mm)
                if (!vc_orange_is_valid(blobs[i], video.width, video.height)) continue;
    
                frame_oranges++;

                // Calcula calibre segundo regulamento CEE 379/71
                int calibre = vc_orange_calibre(blobs[i]);

                // Diâmetro em mm para mostrar no HUD
                float diameter_mm = MAX(blobs[i].width, blobs[i].height) * (55.0f / 280.0f);

                // Bounding box verde
                cv::rectangle(frame,
                    cv::Point(blobs[i].x, blobs[i].y),
                    cv::Point(blobs[i].x + blobs[i].width, blobs[i].y + blobs[i].height),
                    cv::Scalar(0, 255, 0), 2);

                // Centróide vermelho (confirmar depois)
                /*
                cv::circle(frame,
                    cv::Point(blobs[i].xc, blobs[i].yc), 4,
                    cv::Scalar(0, 0, 255), -1);
                */
                cv::rectangle(frame,
                cv::Point(blobs[i].xc - 3, blobs[i].yc - 3),
                cv::Point(blobs[i].xc + 3, blobs[i].yc + 3),
                cv::Scalar(0, 0, 255), -1);;

                // Info por cima de cada laranja
                str = "Cal:" + std::to_string(calibre) +
                      " D:"  + std::to_string((int)diameter_mm) + "mm" +
                      " A:"  + std::to_string(blobs[i].area)    +
                      " P:"  + std::to_string(blobs[i].perimeter);
                cv::putText(frame, str,
                            cv::Point(blobs[i].x, blobs[i].y - 5),
                            cv::FONT_HERSHEY_SIMPLEX, 0.45,
                            cv::Scalar(0, 0, 0), 2);
                cv::putText(frame, str,
                            cv::Point(blobs[i].x, blobs[i].y - 5),
                            cv::FONT_HERSHEY_SIMPLEX, 0.45,
                            cv::Scalar(255, 255, 255), 1);
            }

            free(blobs);
        }

        // Atualiza contagem total acumulada (tracking ainda não implementado!!)
        total_oranges += frame_oranges;

         // HUD - informação básica sobre o vídeo (canto superior esquerdo)
        str = "Frame: " + std::to_string(video.nframe) + "/" +
              std::to_string(video.ntotalframes);
        cv::putText(frame, str, cv::Point(20, 25),
                    cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0,0,0), 2);
        cv::putText(frame, str, cv::Point(20, 25),
                    cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255,255,255), 1);

        str = "Laranjas no frame: " + std::to_string(frame_oranges);
        cv::putText(frame, str, cv::Point(20, 55),
                    cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0,0,0), 2);
        cv::putText(frame, str, cv::Point(20, 55),
                    cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255,255,255), 1);

        str = "Total acumulado: " + std::to_string(total_oranges);
        cv::putText(frame, str, cv::Point(20, 85),
                    cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0,0,0), 2);
        cv::putText(frame, str, cv::Point(20, 85),
                    cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255,255,255), 1);

        // Máscara para debug
        cv::Mat mask_mat(video.height, video.width, CV_8UC1, image_mask->data);
        cv::imshow("VC - MASCARA", mask_mat);
        cv::imshow("VC - ORIGINAL",   frame);
        key = cv::waitKey(1000 / video.fps);
    }

    // libertar memória
    vc_image_free(image_bgr);
    vc_image_free(image_hsv);
    vc_image_free(image_mask);
    vc_image_free(image_tmp);
    vc_image_free(image_label);

    // Fecha a janela 
    cv::destroyAllWindows();
    //Fecha o ficheiro de vídeo 
    capture.release();
    return 0;
}
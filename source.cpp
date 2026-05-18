#include <iostream>
#include <string>
#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/videoio.hpp>
#include <cstring>
#include <algorithm>
#include "functions.h"
//g++ source.cpp functions.cpp -o programa $(pkg-config --cflags --libs opencv4)

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
    //cv::namedWindow("VC - MASCARA", cv::WINDOW_AUTOSIZE); // debug

    // Aloca imagens IVC uma única vez, fora do loop 
    IVC *image_bgr  = vc_image_new(video.width, video.height, 3, 255);
    IVC *image_hsv  = vc_image_new(video.width, video.height, 3, 255);
    IVC *image_mask = vc_image_new(video.width, video.height, 1, 255);
    IVC *image_tmp  = vc_image_new(video.width, video.height, 1, 255);
    IVC *image_label  = vc_image_new(video.width, video.height, 1, 255);

    // Tracking: guarda blobs válidos da frame anterior
    OVC  *prev_blobs  = NULL;
    int   nprev_blobs = 0;
    int   total_oranges = 0;
    const float MAX_DIST = 60.0f;     // px — ajusta ao movimento entre frames

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
        vc_hsv_segmentation(image_hsv, image_mask, 5, 35, 45, 100, 23, 100);

        // não é a melhor segmentação mas faz desaparecer a maçã
        //vc_hsv_segmentation(image_hsv, image_mask, 18, 30, 79, 100, 30, 100);

        // Operadores Morfológicos: Abertura
        vc_binary_erosion(image_mask, image_tmp,  3);
        vc_binary_dilation(image_tmp, image_mask, 3);   

        // Etiquetagem: atribui um ID único a cada blob (laranja separada)
        int nlabels = 0;
        OVC *blobs = vc_binary_blob_labelling(image_mask, image_label, &nlabels);

        // Calcula métricas de cada blob (área, perímetro, bounding box, centróide)
        if (blobs != NULL)
            vc_binary_blob_info(image_label, blobs, nlabels);

        // Filtra só blobs válidos da frame atual
        int  nvalid = 0;
        OVC *valid_blobs = NULL;

        if (blobs != NULL)
        {
            // aloca no pior caso
            valid_blobs = (OVC *)malloc(nlabels * sizeof(OVC));

            for (int i = 0; i < nlabels; i++)
            {
                /*printf("Blob %d | area=%d | bbox=%dx%d | fill=%.2f\n",
                i, blobs[i].area, blobs[i].width, blobs[i].height,
                (float)blobs[i].area / (float)(blobs[i].width * blobs[i].height));*/ //debug

                //Verifica calibre mínimo de 53 mm, fill ratio ≥ 0,70 e visibilidade completa da bounding box, rejeitando objetos com buracos ou cortados.
                if (!vc_orange_is_valid(blobs[i], video.width, video.height)) continue;

                // 0 = ainda não medido o defeito
                blobs[i].defect_measured = 0;

                // Verifica se este blob já foi medido numa frame anterior:
                // se sim, reutiliza o valor — evita variações entre frames/*
                for (int p = 0; p < nprev_blobs; p++)
                {
                    float dx = (float)(blobs[i].xc - prev_blobs[p].xc);
                    float dy = (float)(blobs[i].yc - prev_blobs[p].yc);
                    if (sqrtf(dx*dx + dy*dy) < MAX_DIST && prev_blobs[p].defect_measured)
                    {
                        // Reutiliza medição anterior — não recalcula
                        blobs[i].defect_ratio    = prev_blobs[p].defect_ratio;
                        blobs[i].defect_measured = 1;
                        break;
                    }
                }

                // Só mede defeitos na primeira vez que a bounding box aparece completa no ecrã
                // (vc_orange_is_valid já garante que está completamente visível)
                if (!blobs[i].defect_measured)
                {
                    blobs[i].defect_ratio    = vc_orange_defect_ratio(blobs[i], image_hsv);
                    blobs[i].defect_measured = 1;
                }

                // Calcula calibre segundo regulamento CEE 379/71
                int calibre = vc_orange_calibre(blobs[i]);

                const char *category = vc_orange_category(blobs[i].defect_ratio);
            
                // Diâmetro em mm para mostrar no HUD
                float diameter_mm = MAX(blobs[i].width, blobs[i].height) * (55.0f / 280.0f);

                // Bounding box verde
                cv::rectangle(frame,
                    cv::Point(blobs[i].x, blobs[i].y),
                    cv::Point(blobs[i].x + blobs[i].width, blobs[i].y + blobs[i].height),
                    cv::Scalar(0, 255, 0), 2);

                // Centro vermelho retangular
                cv::rectangle(frame,
                cv::Point(blobs[i].xc - 3, blobs[i].yc - 3),
                cv::Point(blobs[i].xc + 3, blobs[i].yc + 3),
                cv::Scalar(0, 0, 255), -1);;
                
                // Info por cima de cada laranja
                str = "Cal:" + std::to_string(calibre) +
                      " D:"  + std::to_string((int)diameter_mm) + "mm" +
                      " A:"  + std::to_string(blobs[i].area)    +
                      " P:"  + std::to_string(blobs[i].perimeter) +
                      " C:" + std::string(category);
                cv::putText(frame, str,
                            cv::Point(blobs[i].x, blobs[i].y - 5),
                            cv::FONT_HERSHEY_SIMPLEX, 0.45,
                            cv::Scalar(0, 0, 0), 2);
                cv::putText(frame, str,
                            cv::Point(blobs[i].x, blobs[i].y - 5),
                            cv::FONT_HERSHEY_SIMPLEX, 0.45,
                            cv::Scalar(255, 255, 255), 1);
                valid_blobs[nvalid++] = blobs[i];   // guarda blob válido
            }

            free(blobs);
        }

        // Tracking: conta as que saíram desde a frame anterior 
        if (prev_blobs != NULL && nvalid >= 0)
        {
            total_oranges += vc_count_entered_oranges(prev_blobs, nprev_blobs, valid_blobs, nvalid, MAX_DIST);
        }

        // Atualiza estado anterior
        if (prev_blobs != NULL)
            free(prev_blobs);

        prev_blobs  = valid_blobs;   // transfere ownership
        nprev_blobs = nvalid;
        valid_blobs = NULL;          // não libertar aqui — passa para prev

        // HUD - informação básica sobre o vídeo (canto superior esquerdo)
        str = "Frame: " + std::to_string(video.nframe) + "/" +
              std::to_string(video.ntotalframes);
        cv::putText(frame, str, cv::Point(20, 25),
                    cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0,0,0), 2);
        cv::putText(frame, str, cv::Point(20, 25),
                    cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255,255,255), 1);

        str = "Laranjas no frame: " + std::to_string(nvalid);
        cv::putText(frame, str, cv::Point(20, 55),
                    cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0,0,0), 2);
        cv::putText(frame, str, cv::Point(20, 55),
                    cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255,255,255), 1);

        str = "Total laranjas: " + std::to_string(total_oranges);
        cv::putText(frame, str, cv::Point(20, 85),
                    cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0,0,0), 2);
        cv::putText(frame, str, cv::Point(20, 85),
                    cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255,255,255), 1);

        // Máscara para debug
        //cv::Mat mask_mat(video.height, video.width, CV_8UC1, image_mask->data);
        //cv::imshow("VC - MASCARA", mask_mat);
        cv::imshow("VC - ORIGINAL",   frame);
        key = cv::waitKey(1000 / video.fps);
    }

    // Depois do while, antes de libertar memória:
    if (prev_blobs != NULL)
    {
        total_oranges += nprev_blobs;   // laranjas que ficaram no último frame
        free(prev_blobs);
        prev_blobs = NULL;
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
#include "core/ProcessingThread.h"
#include "core/PixelProcessor.h"
#include "core/DefectAlgorithm.h"

ProcessingThread::ProcessingThread(TaskType type, const QImage &inputImage, const QImage &oriImage
                    ,const QImage &standard,QObject *parent)
    : QThread(parent), m_taskType(type), m_inputImage(inputImage),m_oriImage(oriImage),m_standardImage(
        standard) {
}

void ProcessingThread::run() {
    // QThread::sleep(3);
    QImage resultImg;
    QString msg;

    // 根据任务类型，调用对应的底层算法（此时运行在子线程中，不会阻塞UI）
    switch (m_taskType) {
        case LinearStretch: {
                resultImg = PixelProcessor::linearStretch(m_inputImage);
                msg = "线性拉伸完成"; 
                break;
            }
        case HistEq:{
            resultImg = PixelProcessor::histEqualize(m_inputImage);
            msg = "直方图均衡化完成"; 
            break;
        }            
        case Gaussian: {
            resultImg = PixelProcessor::gaussianBlur(m_inputImage); 
            msg = "高斯滤波完成"; 
            break;
        }      
        case Laplacian: {
            resultImg = PixelProcessor::laplacian(m_inputImage); 
            msg = "拉普拉斯锐化完成";
            break;
        }     
        case AdaptiveThresh:{
            resultImg = PixelProcessor::adaptiveThreshold(m_inputImage); 
            msg = "自适应阈值分割完成"; 
            break;
        } 
        case Otsu:{
            resultImg = PixelProcessor::otsuThreshold(m_inputImage); 
            msg = "Otsu分割完成"; 
            break;
        }       
        case MorphOpen: {
            resultImg = PixelProcessor::morphOpen(m_inputImage); 
            msg = "开运算完成"; 
            break;
        }    
        case MorphClose:{
            resultImg = PixelProcessor::morphClose(m_inputImage); 
            msg = "闭运算完成"; 
            break;
        }    
        case Sobel:  {
            resultImg = PixelProcessor::sobel(m_inputImage); 
            msg = "Sobel边缘提取完成"; 
            break;
        }        
        case Prewitt: {
            resultImg = PixelProcessor::prewitt(m_inputImage); 
            msg = "Prewitt边缘提取完成"; 
            break;
        }       
        case Detect: {
            DetectResult res = DefectAlgorithm::detect(m_inputImage,m_standardImage);
            resultImg = res.resultImage; 
            msg = res.message; 
            break;
        }
    }

    // 处理完毕，发射信号将数据安全投递回主线程
    emit resultReady(resultImg, msg);
}

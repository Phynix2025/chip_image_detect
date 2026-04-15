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
        case TwoWayFilter: {
            resultImg = PixelProcessor::twoWayFilter(m_inputImage);
            msg = "双边滤波完成";
            break;
        }
        case Retinex: {
            resultImg = PixelProcessor::Retinex(m_inputImage);
            msg = "Retinex 完成";
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
        /*TemplateMatch,ImageDiff,ThreshSeg,PointLink,DefectAnalysis  // 缺陷检测*/  
        case TemplateMatch: {
            DetectResult res = DefectAlgorithm::templateMatch(m_inputImage, m_standardImage);
            resultImg = res.resultImage;
            msg = res.message;
            break;
        }
        case ImageDiff: {
            DetectResult res = DefectAlgorithm::imageDiff(m_inputImage, m_standardImage);
            resultImg = res.resultImage;
            msg = res.message;
            
            break;
        }
        case ThreshSeg: {
            DetectResult res = DefectAlgorithm::threshSeg(m_inputImage);
            resultImg = res.resultImage;
            msg = res.message;
            
            break;
        }
        case PointLink: {
            DetectResult res = DefectAlgorithm::pointLink(m_inputImage);
            resultImg = res.resultImage;
            msg = res.message;
            
            break;
        }
        case DefectAnalysis: {
            DetectResult res = DefectAlgorithm::defectAnalysis(m_inputImage);
            resultImg = res.resultImage;
            msg = res.message;
            
            break;
        }
        default: {
            msg = "传入线程的操作有误！请重试。";
            break;
        }

    }

    // 处理完毕，发射信号将数据安全投递回主线程
    emit resultReady(resultImg, msg);
}

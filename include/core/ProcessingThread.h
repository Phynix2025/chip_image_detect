#ifndef PROCESSINGTHREAD_H
#define PROCESSINGTHREAD_H

#include <QThread>
#include <QImage>
#include <QString>
#include <qimage.h>

class ProcessingThread : public QThread {
    Q_OBJECT

public:

    // 定义任务类型枚举，复用同一个线程类
    enum TaskType {
        LinearStretch, HistEq, Gaussian, Laplacian, TwoWayFilter,Retinex, // 预处理
        AdaptiveThresh, Otsu,                        // 分割
        MorphOpen, MorphClose, Sobel, Prewitt,       // 特征提取
        TemplateMatch,ImageDiff,ThreshSeg,PointLink,DefectAnalysis  // 缺陷检测
    };

    ProcessingThread(TaskType type, const QImage &inputImage, const QImage &oriImage
                    ,const QImage &standard,QObject *parent = nullptr);

protected:
    // 重写 run() 函数，这是子线程的实际执行体
    void run() override;

signals:
    // 跨线程传递结果
    void resultReady(QImage resultImage, QString message);

private:
    TaskType m_taskType;
    QImage m_inputImage;
    QImage m_standardImage; //标准图像
    QImage m_oriImage; //原始图像
};

#endif // PROCESSINGTHREAD_H
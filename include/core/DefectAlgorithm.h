#ifndef DEFECTALGORITHM_H
#define DEFECTALGORITHM_H

#include <QImage>
#include <QString>
#include <qimage.h>

// 用于统一返回检测后的图像和文字结果
struct DetectResult {
    QImage resultImage;
    QString message;
};

class DefectAlgorithm {
public:
    //1. 模板匹配：匹配后对待测图裁剪 参考 QImage croppedImage = originalImage.copy(x, y, width, height);
    static DetectResult templateMatch(QImage &input,QImage &standard);
    //2. 图像差分 
    static DetectResult imageDiff(const QImage &input,const QImage &standard);
    //3. 阈值分割
    static DetectResult threshSeg(const QImage &input);
    //4. 断点连接
    static DetectResult pointLink(const QImage &input);
    //5. 缺陷分析（连通域分析）
    static DetectResult defectAnalysis(const QImage &input);

private:
    

};

#endif // DEFECTALGORITHM_H
#ifndef DEFECTALGORITHM_H
#define DEFECTALGORITHM_H

#include <QImage>
#include <QString>
#include <qimage.h>
#include <algorithm>

// 定义连通域的特征
struct ComponentStats {
    int labelId;
    int area = 0;
    int minX = INT_MAX, maxX = -1;
    int minY = INT_MAX, maxY = -1;
    
    // 计算外接矩形的长宽比 (永远用长边除以短边，保证比例 >= 1.0)
    double getAspectRatio() const {
        int width = maxX - minX + 1;
        int height = maxY - minY + 1;
        int longSide = std::max(width, height);
        int shortSide = std::min(width, height);
        // 防止除零错误（虽然面积>0时短边肯定>=1）
        if (shortSide == 0) return 1.0; 
        return static_cast<double>(longSide) / shortSide;
    }
};

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
    //4. 连通域分析
    static DetectResult connectivityAnalysis(const QImage &input);
    //5. 缺陷分析
    static DetectResult defectAnalysis(const QImage &input);

private:
    
    // 辅助函数
    // 获取标签矩阵
    static void getLabelMatrix(const QImage &input,std::vector<std::vector<int>> &labelMatrix);
};

#endif // DEFECTALGORITHM_H

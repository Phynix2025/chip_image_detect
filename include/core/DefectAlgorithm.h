#ifndef DEFECTALGORITHM_H
#define DEFECTALGORITHM_H

#include <QImage>
#include <QString>
#include <qimage.h>
#include <algorithm>
#include <vector>

// 定义连通域的特征
struct ComponentStats {
    int labelId;
    int area = 0;
    int minX = INT_MAX, maxX = -1;
    int minY = INT_MAX, maxY = -1;

    // 用于记录分类结果（例如：0=未知, 1=划痕, 2=崩边, 3=异物）
    int defectType = 0;
    
    // 计算外接矩形的长宽比 (永远用长边除以短边，保证比例 >= 1.0)
    double getAspectRatio() const {
        int width = maxX - minX + 1;
        int height = maxY - minY + 1;
        int longSide = std::max(width, height);
        int shortSide = std::min(width, height);
        // 防止除零错误
        if (shortSide == 0) return 1.0; 
        return static_cast<double>(longSide) / shortSide;
    }
    // 计算外接矩形填充率
    double getExtent() const {
        int width = maxX - minX + 1;
        int height = maxY - minY + 1;
        int boundingBoxArea = width * height;
        if (boundingBoxArea == 0) return 1.0;
        return static_cast<double>(area) / boundingBoxArea;
    }
};

// 用于统一返回检测后的图像和文字结果
struct DetectResult {
    QImage resultImage;
    QString message;
    // 需要传递的标签矩阵
    std::vector<std::vector<int>> labelMatrix;
    // 筛选后的有效缺陷特征
    std::vector<ComponentStats> validDefects;
};

class DefectAlgorithm {
public:
    //1. 模板匹配：匹配后对待测图裁剪 参考 QImage croppedImage = originalImage.copy(x, y, width, height);
    static DetectResult templateMatch(QImage &input,QImage &standard);
    //2. 图像差分 
    static DetectResult imageDiff(const QImage &input,const QImage &standard);
    //3. 阈值分割
    static DetectResult threshSeg(const QImage &input,QImage &standard);
    //4. 连通域分析
    static DetectResult connectivityAnalysis(const QImage &input,DetectResult &curRes,
                                            QImage &standard);
    //5. 缺陷分析
    static DetectResult defectAnalysis(const QImage &input,DetectResult &curRes);

private:
    // 辅助函数
    // 获取标签矩阵
    static void getLabelMatrix(const QImage &input,std::vector<std::vector<int>> &labelMatrix);
    // 特征提取
    static QImage featureExtraction(const QImage &input,std::vector<std::vector<int>> &labelMatrix,
        std::vector<ComponentStats> &validDefects);
    static void classifyFeatures(std::vector<ComponentStats> &defects,
                                                  int imgWidth, int imgHeight);
    static void globalAnalysis(const std::vector<ComponentStats> &defects, QString &message);
    static QImage visualizeResults(const QImage &input, const std::vector<ComponentStats> &defects);
};

#endif // DEFECTALGORITHM_H

#include "core/DefectAlgorithm.h"
#include "core/PixelProcessor.h"

#include <climits>
#include <cstdlib>
#include <qimage.h>
#include <qtypes.h>
#include <QImage>
#include <QPoint>
#include <cmath>
#include <algorithm>

// 暴力匹配
QImage forceTemplateMatch(const QImage &input,const QImage &standard);
// 图像金字塔
QImage pyramidTemplateMatch(const QImage &input, const QImage &standard);

DetectResult DefectAlgorithm::templateMatch(QImage &input,QImage &standard){
    DetectResult res;
    // 优化：在模板匹配之前预处理，减少噪点光照等因素干扰
    
    res.resultImage = pyramidTemplateMatch(input, standard);
    res.message = "模板匹配完成";
    return res;
}

DetectResult DefectAlgorithm::imageDiff(const QImage &input,const QImage &standard){
    DetectResult res;
    
    res.message = "图像差分完成";
    return res;
}

DetectResult DefectAlgorithm::threshSeg(const QImage &input){
    DetectResult res;
    
    res.message = "阈值分割完成";
    return res;
}

DetectResult DefectAlgorithm::pointLink(const QImage &input){
    DetectResult res;
    
    res.message = "断点连接完成";
    return res;
}

DetectResult DefectAlgorithm::defectAnalysis(const QImage &input){
    DetectResult res;
    
    res.message = "无缺陷/有缺陷,缺陷类型是: xx";
    return res;
}



// （辅助函数）用于在一个小范围内精确匹配
QPoint matchInRegion(const QImage &input, const QImage &standard,
                     int startX, int endX, int startY, int endY) {
    int minSad = INT_MAX;
    QPoint bestPos(startX, startY); // 默认为起始点

    const int sw = standard.width();
    const int sh = standard.height();
    const int sBpl = standard.bytesPerLine();
    const int iBpl = input.bytesPerLine();
    const uchar *pStd = standard.constBits();
    const uchar *pIpt = input.constBits();

    // 检查参数是否合法
    startX = std::max(0, startX);
    startY = std::max(0, startY);
    endX = std::min(input.width() - sw, endX);
    endY = std::min(input.height() - sh, endY);

    for (int y = startY; y <= endY; ++y) {
        for (int x = startX; x <= endX; ++x) {
            int diff = 0;
            for (int i = 0; i < sh; ++i) {
                // 指针定位到当前行
                const uchar *rowStd = pStd + i * sBpl;
                const uchar *rowIpt = pIpt + (y + i) * iBpl + x;
                for (int j = 0; j < sw; ++j) {
                    diff += std::abs(rowStd[j] - rowIpt[j]);
                }
                // 小优化：如果还没算完就已经比 minSad 大了，剪枝
                if (diff >= minSad) break;
            }
            if (diff < minSad) {
                minSad = diff;
                bestPos.setX(x);
                bestPos.setY(y);
            }
        }
    }
    return bestPos;
}

QImage pyramidTemplateMatch(const QImage &input, const QImage &standard) {

    // 第一步：构建3层图像金字塔 
    // L0: 原图
    QImage inL0 = input;
    QImage stdL0 = standard;

    // L1: 缩小 1/2 
    QImage inL1 = inL0.scaled(inL0.width() / 2, inL0.height() / 2, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    QImage stdL1 = stdL0.scaled(stdL0.width() / 2, stdL0.height() / 2, Qt::IgnoreAspectRatio, Qt::FastTransformation);

    // L2: 缩小 1/4
    QImage inL2 = inL1.scaled(inL1.width() / 2, inL1.height() / 2, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    QImage stdL2 = stdL1.scaled(stdL1.width() / 2, stdL1.height() / 2, Qt::IgnoreAspectRatio, Qt::FastTransformation);

    // 在金字塔顶层 (L2) 进行全图搜索
    QPoint bestL2 = matchInRegion(inL2, stdL2, 0, inL2.width() - stdL2.width(), 0, inL2.height() - stdL2.height());

    // 将 L2 找到的坐标放大 2 倍，作为 L1 的大致位置
    int guessX1 = bestL2.x() * 2;
    int guessY1 = bestL2.y() * 2;
    
    // 微调的搜索半径
    int searchRadius = 3; 

    QPoint bestL1 = matchInRegion(inL1, stdL1, 
                                  guessX1 - searchRadius, guessX1 + searchRadius,
                                  guessY1 - searchRadius, guessY1 + searchRadius);


    //向最底层 (L0, 原图) 投影并微调=
    // 将 L1 找到的坐标再放大 2 倍
    int guessX0 = bestL1.x() * 2;
    int guessY0 = bestL1.y() * 2;

    // 在原图层进行最后的像素级精确定位
    QPoint bestL0 = matchInRegion(inL0, stdL0,
                                  guessX0 - searchRadius, guessX0 + searchRadius,
                                  guessY0 - searchRadius, guessY0 + searchRadius);


    //裁剪
    int finalX = bestL0.x();
    int finalY = bestL0.y();
    int finalW = standard.width();
    int finalH = standard.height();

    // 安全检查，防止 copy 时越界
    finalX = std::max(0, std::min(finalX, input.width() - finalW));
    finalY = std::max(0, std::min(finalY, input.height() - finalH));

    return input.copy(finalX, finalY, finalW, finalH);
}

// 暴力匹配 时间很长
QImage forceTemplateMatch(const QImage &input, const QImage &standard) {

    int minV = INT_MAX;
    int matchX = 0, matchY = 0; // 赋初值，防止找不到时变成野值
    int hStandard = standard.height(), wStandard = standard.width();
    int hInput = input.height(), wInput = input.width();

    // 提到底层循环外部，避免重复调用函数，提升性能
    const uchar *pStd = standard.constBits();
    const uchar *pIpt = input.constBits();
    int stdBpl = standard.bytesPerLine();
    int iptBpl = input.bytesPerLine();

    // 优化.外层循环 y，内层循环 x，符合图像在内存中按行存储的顺序，大幅提升读取速度
    for(int y = 0; y <= hInput - hStandard; ++y) {
        for(int x = 0; x <= wInput - wStandard; ++x) {
            int diff = 0;
            for(int i = 0; i < hStandard; ++i) {
                for(int j = 0; j < wStandard; ++j) {
                    int offset1 = stdBpl * i + j; 
                    int offset2 = iptBpl * (y + i) + (x + j);
                    diff += std::abs(pStd[offset1] - pIpt[offset2]);
                }
            }
            
            if(diff < minV) {
                minV = diff;
                matchX = x;
                matchY = y;
            }
        }
    }

    // 裁剪图像目标区域
    QImage res = input.copy(matchX, matchY, wStandard, hStandard);

    return res;  
}

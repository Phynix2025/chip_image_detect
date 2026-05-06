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
#include <vector>
#include <unordered_map>

/* 辅助函数 */
// 暴力匹配
QImage forceTemplateMatch(const QImage &input,const QImage &standard);
// 图像金字塔
QImage pyramidTemplateMatch(const QImage &input, const QImage &standard);
// 逐像素作差
QImage perBytesDiff(const QImage &input,const QImage &standard);
// 局部均值作差
QImage localMeanDiff(const QImage &input,const QImage &standard);

/* 计算主函数 */
// 模板匹配
DetectResult DefectAlgorithm::templateMatch(QImage &input,QImage &standard){
    DetectResult res;
    // 优化：在模板匹配之前预处理，减少噪点光照等因素干扰
    standard = PixelProcessor::twoWayFilter(standard);
    standard = PixelProcessor::Retinex(standard);
    standard = PixelProcessor::sobel(standard);

    input = PixelProcessor::twoWayFilter(input);
    input = PixelProcessor::Retinex(input);
    input = PixelProcessor::sobel(input);


    res.resultImage = pyramidTemplateMatch(input, standard);
    res.message = "模板匹配完成(已预处理)";

    return res;
}

// 图像差分
DetectResult DefectAlgorithm::imageDiff(const QImage &input,const QImage &standard){
    DetectResult res;
    //res.resultImage = perBytesDiff(input, standard);
    res.resultImage = localMeanDiff(input, standard);
    res.message = "图像差分完成";
    return res;
}

DetectResult DefectAlgorithm::threshSeg(const QImage &input){
    DetectResult res;
    res.resultImage = PixelProcessor::fixedThreshold(input,50);
    res.message = "阈值分割完成";
    return res;
}

DetectResult DefectAlgorithm::connectivityAnalysis(const QImage &input){
    DetectResult res;
    
    // 1. 获得标签矩阵
    int h = input.height(),w = input.width();
    std::vector<std::vector<int>> labelMatrix(h,std::vector(w,0));
    getLabelMatrix(input,labelMatrix);

    // 2. 特征提取
    

    res.message = "连通域分析完成";
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


    //向最底层 (L0, 原图) 投影并微调
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

// 逐像素作差
QImage perBytesDiff(const QImage &input,const QImage &standard){
    QImage res = input.copy();
    // 图像大小
    const int wStd = standard.width(),hStd = standard.height();
    const int wIpt = input.width(),hIpt = input.height();
    // 差分图大小，防止指针越界（虽然正常处理不会越界，但是防止用户有错误操作）
    const int wRes = std::min(wStd,wIpt),hRes = std::min(hStd,hIpt);
    // 每一行内存字节数
    const int lRes = std::min(input.bytesPerLine(),standard.bytesPerLine()); 

    const uchar *pIpt = input.constBits();
    const uchar *pStd = standard.constBits();
    uchar *pRes = res.bits();
    
    for(int y = 0; y < hRes; ++ y){
        for(int x = 0; x < wRes; ++ x){
            int offset = x + y * lRes; // 指针偏移量
            //pRes[offset] = std::abs(static_cast<int>(pIpt[offset]) - pStd[offset]);

            int tmp1 = static_cast<int>(pIpt[offset]);
            int tmp2 = static_cast<int>(pStd[offset]);
            //由于芯片是暗色的，缺陷是相对明亮的，所以采用正向截断法
            pRes[offset] =static_cast<uchar>(std::min(255,std::max(0,tmp1 - tmp2))) ;
        }
    }

    // 后处理：去除小影响,噪点
    res = PixelProcessor::twoWayFilter(res);

    return res;
}

// 局部均值作差
QImage localMeanDiff(const QImage &input, const QImage &standard) {
    // 这里假设已经金字塔对齐并裁剪好了
    int width = std::min(input.width(), standard.width());
    int height = std::min(input.height(), standard.height());

    // 由于差分图计算时跳过边缘，所以初始化为一张全黑图
    QImage res(width, height, QImage::Format_Grayscale8);
    res.fill(0); // 全部填充为黑色

    // 三张图的独立换行步长
    int iptBpl = input.bytesPerLine();
    int stdBpl = standard.bytesPerLine();
    int resBpl = res.bytesPerLine();

    const uchar *pIpt = input.constBits();
    const uchar *pStd = standard.constBits();
    uchar *pRes = res.bits();
    
    // 跳过边缘 
    for(int y = 1; y < height - 1; ++y) {
        for(int x = 1; x < width - 1; ++x) {
            int oIpt = y * iptBpl + x;
            int oStd = y * stdBpl + x;
            int oRes = y * resBpl + x;

            // 计算均值 
            int meanIpt = (pIpt[oIpt - iptBpl] + pIpt[oIpt + iptBpl] + 
                           pIpt[oIpt - 1] + pIpt[oIpt + 1] + pIpt[oIpt]) / 5;
                           
            int meanStd = (pStd[oStd - stdBpl] + pStd[oStd + stdBpl] + 
                           pStd[oStd - 1] + pStd[oStd + 1] + pStd[oStd]) / 5;
            
            meanIpt = std::min(255,std::max(0,meanIpt - meanStd));
            pRes[oRes] = static_cast<uchar>(meanIpt);
        }
    }

    // 去除小噪点
    res = PixelProcessor::twoWayFilter(res);

    return res;
}

// 辅助函数：并查集的 Find 操作（带路径压缩）
int findRoot(std::vector<int>& parent, int i) {
    if (parent[i] == i) {
        return i;
    }
    // 路径压缩：直接将当前节点挂到根节点下，极大提升后续查找速度
    return parent[i] = findRoot(parent, parent[i]);
}

// 辅助函数：并查集的 Union 操作
void unionLabels(std::vector<int>& parent, int i, int j) {
    int rootI = findRoot(parent, i);
    int rootJ = findRoot(parent, j);
    if (rootI != rootJ) {
        // 将较大的根节点指向较小的根节点，保持标签值尽量小
        if (rootI < rootJ) {
            parent[rootJ] = rootI;
        } else {
            parent[rootI] = rootJ;
        }
    }
}

void DefectAlgorithm::getLabelMatrix(const QImage &input, std::vector<std::vector<int>> &labelMatrix) {
    int width = input.width();
    int height = input.height();

    // 1. 初始化标签矩阵和并查集
    // 矩阵大小初始化为 height x width，全部填 0（背景）
    labelMatrix.assign(height, std::vector<int>(width, 0));
    
    std::vector<int> parent;
    parent.push_back(0); // 索引 0 保留给背景，不参与并查集逻辑

    int nextLabel = 1;

    // 2. 第一遍扫描 (First Pass)
    for (int y = 0; y < height; ++y) {
        // 【核心优化】：获取当前行的只读内存指针，速度比 pixel() 快几十倍
        const uchar* line = input.constScanLine(y); 
        
        for (int x = 0; x < width; ++x) {
            // 注意：这里假设输入的 QImage 是 8位灰度图 (Format_Grayscale8 或 Format_Indexed8)
            // 如果你的图是 32位 RGB，需要改成：int pixelVal = qRed(((QRgb*)line)[x]);
            int pixelVal = line[x];

            // 假设前景（划痕/焊盘）为白色 (值 > 128)
            if (pixelVal > 128) { 
                int leftLabel = (x > 0) ? labelMatrix[y][x - 1] : 0;
                int topLabel  = (y > 0) ? labelMatrix[y - 1][x] : 0;

                if (leftLabel == 0 && topLabel == 0) {
                    // 情况 A：孤立点，左上都没标签，分配新标签
                    labelMatrix[y][x] = nextLabel;
                    parent.push_back(nextLabel);
                    nextLabel++;
                } else if (leftLabel != 0 && topLabel == 0) {
                    // 情况 B1：只有左边有标签
                    labelMatrix[y][x] = leftLabel;
                } else if (leftLabel == 0 && topLabel != 0) {
                    // 情况 B2：只有上边有标签
                    labelMatrix[y][x] = topLabel;
                } else {
                    // 情况 C：左边和上边都有标签（遭遇合并）
                    labelMatrix[y][x] = std::min(leftLabel, topLabel);
                    if (leftLabel != topLabel) {
                        // 记录连通域的等价关系
                        unionLabels(parent, leftLabel, topLabel);
                    }
                }
            }
        }
    }

    // 3. 整理并查集，并压缩标签使之连续 (Continuous Labeling)
    // 这一步能把 [1, 2, 5, 8] 这种断层的标签映射为 [1, 2, 3, 4]
    std::vector<int> finalLabels(parent.size(), 0);
    int currentValidLabel = 1;
    for (int i = 1; i < parent.size(); ++i) {
        int root = findRoot(parent, i);
        if (finalLabels[root] == 0) {
            finalLabels[root] = currentValidLabel++; // 发现新的有效根，分配连续编号
        }
        finalLabels[i] = finalLabels[root]; // 让所有子节点直接指向连续编号
    }

    // 4. 第二遍扫描 (Second Pass) - 贴上正式标签
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (labelMatrix[y][x] > 0) {
                // 通过查表，直接替换为最终的连续标签
                labelMatrix[y][x] = finalLabels[labelMatrix[y][x]];
            }
        }
    }
}

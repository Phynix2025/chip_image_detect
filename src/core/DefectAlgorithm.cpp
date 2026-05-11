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
#include <unordered_set>
#include <QPainter>
#include <QPen>

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
    //standard = PixelProcessor::sobel(standard);

    input = PixelProcessor::twoWayFilter(input);
    input = PixelProcessor::Retinex(input);
    //input = PixelProcessor::sobel(input);

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

DetectResult DefectAlgorithm::threshSeg(const QImage &input,QImage &standard){
    DetectResult res;
    res.resultImage = PixelProcessor::otsuThreshold(input);
    //同时对standard进行阈值分割
    standard = PixelProcessor::otsuThreshold(standard);
    res.message = "阈值分割完成";
    return res;
}

DetectResult DefectAlgorithm::connectivityAnalysis(const QImage &input,DetectResult &curRes,
                                                    QImage &standard){

    // 1. 获得标签矩阵
    getLabelMatrix(input,curRes.labelMatrix);

    // 2. 特征提取，去除小噪点，得到只含有可能缺陷的图像
    curRes.resultImage = featureExtraction(input,curRes.labelMatrix,curRes.validDefects);
    // 同时对标准图连通域分析
    std::vector<std::vector<int>> stdLabelMatrix;
    getLabelMatrix(standard,stdLabelMatrix);
    std::vector<ComponentStats> stdValidDefects;
    standard = featureExtraction(standard,stdLabelMatrix,stdValidDefects);
    curRes.message = "连通域分析完成";
    return curRes;
}

DetectResult DefectAlgorithm::defectAnalysis(const QImage &input, DetectResult &curRes) {
    // 1. 特征分类：基于先验规则对提取出的连通域进行定性
    classifyFeatures(curRes.validDefects, input.width(), input.height());

    // 2. 全局分析：根据分类结果与面积，下达 OK/NG 判决
    // 建议在 DetectResult 结构体中加一个 bool isOK; 字段，这里我们先用 message 承载结论
    globalAnalysis(curRes.validDefects, curRes.message);

    // 3. 结果可视化：在原图上绘制彩色边界框（Bounding Box）
    curRes.resultImage = visualizeResults(input, curRes.validDefects);

    return curRes;
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
    int searchRadius = 5;

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

void DefectAlgorithm::getLabelMatrix(const QImage &input,std::vector<std::vector<int>> &labelMatrix) {
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

QImage DefectAlgorithm::featureExtraction(const QImage &input,
                                          std::vector<std::vector<int>> &labelMatrix,
                                          std::vector<ComponentStats> &validDefects) {
    int height = input.height();
    int width = input.width();

    // 初始化输出图像，全黑背景
    QImage output(width, height, QImage::Format_Grayscale8);
    output.fill(0);

    // 1. 遍历标签矩阵，统计每个连通域的物理特征
    std::unordered_map<int, ComponentStats> statsMap;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int label = labelMatrix[y][x];
            if (label == 0) continue; // 假设 0 为背景，直接跳过

            // 如果是新发现的连通域，初始化其 labelId
            if (statsMap.find(label) == statsMap.end()) {
                statsMap[label].labelId = label;
            }

            // 累加面积并更新外接矩形的极值坐标
            statsMap[label].area++;
            statsMap[label].minX = std::min(statsMap[label].minX, x);
            statsMap[label].maxX = std::max(statsMap[label].maxX, x);
            statsMap[label].minY = std::min(statsMap[label].minY, y);
            statsMap[label].maxY = std::max(statsMap[label].maxY, y);
        }
    }

    // 2. 缺陷规则筛选
    int MIN_DEFECT_AREA = 320;        // 面积下限：滤除微小噪点和月牙状芯片引脚
    int MAX_DEFECT_AREA = 150000;      // 面积上限：滤除大块的背景误判
    double MAX_ASPECT_RATIO = 15.0;   // 长宽比上限：滤除极度细长的非缺陷干扰

    std::unordered_set<int> validLabels; // 存放判定为真实缺陷的标签 ID

    // 【重要安全防范】：清空外部传入的 vector，防止多次调用时数据累积脏乱
    validDefects.clear();

    for (const auto& pair : statsMap) {
        const ComponentStats& stats = pair.second;

        // 形态学约束判定
        if (stats.area >= MIN_DEFECT_AREA &&
            stats.area <= MAX_DEFECT_AREA &&
            stats.getAspectRatio() <= MAX_ASPECT_RATIO) {

            // 记录到 Set 中，方便步骤 3 快速查表重建图像
            validLabels.insert(stats.labelId);

            // 【核心修复】：把合格的特征实体完整打包，传给外部的 validDefects 数组！
            validDefects.push_back(stats);
        }
    }

    // 3. 根据筛选出的有效连通域，重建纯净的缺陷二值图
    for (int y = 0; y < height; ++y) {
        uchar* line = output.scanLine(y);
        for (int x = 0; x < width; ++x) {
            int label = labelMatrix[y][x];
            // 如果该像素属于被保留的有效连通域，则设为前景(白色)
            if (validLabels.count(label) > 0) {
                line[x] = 255;
            }
        }
    }

    return output;
}

// 辅助函数 1：特征分类 (基于规则的专家系统)
void DefectAlgorithm::classifyFeatures(std::vector<ComponentStats> &defects, int imgWidth, int imgHeight) {
    for (auto &defect : defects) {
        double ratio = defect.getAspectRatio();
        double extent = defect.getExtent(); // 获取填充率

        int width = defect.maxX - defect.minX + 1;
        int height = defect.maxY - defect.minY + 1;
        int maxSide = std::max(width, height); // 获取最长边的跨度

        // 边缘约束判定
        bool nearEdge = (defect.minX < 5 || defect.minY < 5 ||
                         defect.maxX > imgWidth - 5 || defect.maxY > imgHeight - 5);

        if (nearEdge) {
            defect.defectType = 2; // 类别 2：崩边 (致命缺陷)
        }
        // 【核心修改】划痕判定双保险：
        // 1. 横平竖直的划痕 (ratio > 3.0)
        // 2. 对角线斜划痕 (跨度很大 maxSide > 50，但内部极度空洞 extent < 0.2)
        else if (ratio > 3.0 || (maxSide > 50 && extent < 0.20)) {
            defect.defectType = 1; // 类别 1：划痕
        }
        else {
            defect.defectType = 3; // 类别 3：异物/表面污点
        }
    }
}

// 辅助函数 2：全局分析与判定
void DefectAlgorithm::globalAnalysis(const std::vector<ComponentStats> &defects, QString &message) {
    if (defects.empty()) {
        message = "检测合格 (OK)：完美品，未发现有效缺陷。";
        return;
    }

    int totalDefectArea = 0;
    bool hasCriticalDefect = false;

    // 遍历统计全局受损情况
    for (const auto &defect : defects) {
        totalDefectArea += defect.area;
        if (defect.defectType == 2) {
            hasCriticalDefect = true; // 只要有 1 处崩边，即判死刑
        }
    }

    // 综合判定逻辑
    if (hasCriticalDefect) {
        message = "检测不合格 (NG)：缺陷区域太多！";
    } else if (totalDefectArea > 3000) {
        message = QString("检测不合格 (NG)：表面缺陷总面积超标 (%1 px)。").arg(totalDefectArea);
    } else {
        message = QString("检测不合格 (NG)：存在 %1 处划痕。").arg(defects.size());
    }
}

// 辅助函数 3：结果可视化输出
QImage DefectAlgorithm::visualizeResults(const QImage &input, const std::vector<ComponentStats> &defects) {
    // 将原图转为 RGB 格式，以便绘制彩色标注
    QImage output = input.convertToFormat(QImage::Format_RGB888);
    QPainter painter(&output);
    painter.setRenderHint(QPainter::Antialiasing); // 开启抗锯齿

    for (const auto &defect : defects) {
        // 构建外接矩形
        QRect rect(defect.minX, defect.minY, defect.maxX - defect.minX + 1, defect.maxY - defect.minY + 1);

        // 根据缺陷类型选用不同的画笔颜色
        QString typeLabel;
        if (defect.defectType == 1) {
            painter.setPen(QPen(Qt::yellow, 2));
            typeLabel = "Scratch";
        } else if (defect.defectType == 2) {
            painter.setPen(QPen(Qt::red, 3));
            typeLabel = "Chipping";
        } else {
            painter.setPen(QPen(Qt::red, 2));
            typeLabel = "划痕";
        }

        // 绘制矩形框
        painter.drawRect(rect);

        // 在矩形框左上角绘制文本标签
        painter.drawText(rect.topLeft() + QPoint(0, -5), typeLabel);
    }
    return output;
}

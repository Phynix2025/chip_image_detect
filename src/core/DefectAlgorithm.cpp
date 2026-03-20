#include "core/DefectAlgorithm.h"
#include "core/PixelProcessor.h"

#include <climits>
#include <cstdlib>
#include <qimage.h>
#include <qtypes.h>

//给静态成员变量赋予初值,分配空间
int DefectAlgorithm::matchX = 0;
int DefectAlgorithm::matchY = 0;

//采用模版匹配
DetectResult DefectAlgorithm::detect(const QImage &input,const QImage &standard) {
    DetectResult res;
    //采用逐像素匹配方式
    //记得把 input 改为灰度图再交给子任务处理
    QImage gray;
    if (input.format() == QImage::Format_Grayscale8) {
        gray = input.copy();
    }else {
        gray = input.convertToFormat(QImage::Format_Grayscale8);
    }
    res.resultImage = simpleTemplateMatch(gray, standard);
    res.message = "检测完成: 发现异常点 (占位测试)";
    return res;
}

QImage DefectAlgorithm::simpleTemplateMatch(const QImage &input, const QImage &standard) {

    int minV = INT_MAX;
    int matchX = 0, matchY = 0; // 赋初值，防止找不到时变成野值
    int hStandard = standard.height(), wStandard = standard.width();
    int hInput = input.height(), wInput = input.width();

    // 提到底层循环外部，避免重复调用函数，提升性能
    const uchar *pStd = standard.constBits();
    const uchar *pIpt = input.constBits();
    int stdBpl = standard.bytesPerLine();
    int iptBpl = input.bytesPerLine();

    // 【优化】外层循环 y，内层循环 x，符合图像在内存中按行存储的顺序，大幅提升读取速度
    for(int y = 0; y <= hInput - hStandard; ++y) {
        for(int x = 0; x <= wInput - wStandard; ++x) {
            int diff = 0;
            for(int i = 0; i < hStandard; ++i) {
                for(int j = 0; j < wStandard; ++j) {
                    int offset1 = stdBpl * i + j; 
                    int offset2 = iptBpl * (y + i) + (x + j); // 加括号逻辑更清晰
                    diff += std::abs(pStd[offset1] - pIpt[offset2]);
                }
            }
            
            // 【修复 Bug 1】：记录最小差值的同时，必须更新 minV
            if(diff < minV) {
                minV = diff;
                matchX = x;
                matchY = y;
            }
        }
    }
    
    // -------- 测试：绘制白色矩形框 --------
    QImage res = input.copy();
    uchar *data = res.bits();
    int resBpl = res.bytesPerLine();

    // 【修复 Bug 2】：边界坐标需要 -1，防止刚好在图像边缘时越界
    int rightEdge = matchX + wStandard - 1;
    int bottomEdge = matchY + hStandard - 1;

    // 画上下边缘
    for(int i = matchX; i <= rightEdge; ++i) {
        data[matchY * resBpl + i] = 255;      // 上边
        data[bottomEdge * resBpl + i] = 255;  // 下边
    }
    // 画左右边缘
    for(int i = matchY; i <= bottomEdge; ++i) {
        data[i * resBpl + matchX] = 255;      // 左边
        data[i * resBpl + rightEdge] = 255;   // 右边
    }

    return res;
}

QImage DefectAlgorithm::diff(const QImage &input,const QImage &standard ){
    //对目标区域做差分
    QImage res = input.copy();
    uchar *data = res.bits();
    const uchar *pStd = standard.constBits();
    int height = standard.height(),width = standard.width();
    for(int i = 0; i < height ; ++ i){
        for(int j = 0; j < width; ++ j){
            int offset1 = standard.bytesPerLine() * i + j; //标准图指针偏移
            int offset2 = res.bytesPerLine() * (i + matchX) + matchY + j; //缺陷图指针偏移

            data[offset2] = std::abs(data[offset2] - pStd[offset1]);
        }
    }

    return res;
}
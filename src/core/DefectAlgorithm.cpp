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

QImage DefectAlgorithm::simpleTemplateMatch(const QImage &input,const QImage &standard){
    //模板与模板对应位置逐像素作差，记录左上角位置，所有像素差的和最小值对应的就是最佳位置
    int minV = INT_MAX;
    int hStandard = standard.height(),wStandard = standard.width();
    int hInput = input.height(),wInput = input.width();
    for(int x = 0; x <= wInput - wStandard; ++ x){
        for(int y = 0; y <= hInput - hStandard; ++ y){
            //对于第一个位置
            const uchar *pStd = standard.constBits();
            const uchar *pIpt = input.constBits();
            int diff = 0;
            for(int i = 0; i < hStandard; ++ i){
                for(int j = 0; j < wStandard; ++ j){
                    int offset1 = standard.bytesPerLine() * i + j; //模板图的指针偏移
                    int offset2 = input.bytesPerLine() * (y + i) + j + x; //input 的指针偏移
                    diff += std::abs(pStd[offset1] - pIpt[offset2]);
                }
            }
            //这个位置计算完后
            if(diff < minV){
                matchX = x;
                matchY = y;
            }
        }
    }
    
    //测试
    QImage res = input.copy();
    uchar *data = res.bits();
    for(int i = matchX; i < matchX + wStandard; ++ i){
        data[matchY * res.bytesPerLine() + i] = 255;
    }
    for(int i = matchX; i < matchX + wStandard; ++ i){
        data[(matchY + hStandard )* res.bytesPerLine() + i] = 255;
    }
    for(int i = matchY; i < matchY + hStandard; ++ i){
        data[i * res.bytesPerLine() + matchX] = 255;
    }
    for(int i = matchY; i < matchY + hStandard; ++ i){
        data[i * res.bytesPerLine() + matchX + wStandard] = 255;
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
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
    static DetectResult detect(const QImage &input,const QImage &standard);

private:
    //算法私有的辅助函数
    //1. 图像对齐，简单模板匹配（暂时不考虑旋转）
    // 得到偏移量（x,y）等价于 模板最匹配输入时的左上角位置
    static QImage simpleTemplateMatch(const QImage &input,const QImage &standard);
    //2. 图像差分，将目标区域和模板图像素相减，计算绝对差值,得到差分图
    static QImage diff(const QImage &input,const QImage &standard );

    //3. 对目标区域阈值分割

    //4. 目标区域 开运算 闭运算

    //5. 目标区域连通域分析 - 直接在 input 上画白色矩形表示缺陷位置

    //图像目标区域的左上角坐标
    static int matchX,matchY;

};

#endif // DEFECTALGORITHM_H
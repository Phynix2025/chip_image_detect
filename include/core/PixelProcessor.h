#ifndef PIXELPROCESSOR_H
#define PIXELPROCESSOR_H

#include <QImage>
#include <QString>

class PixelProcessor {
public:
    static bool saveImage(const QImage &image, const QString &filePath);
    
    // 预处理
    /* @brief 分段线性拉伸,两点分三段
    */
    static QImage linearStretch(const QImage &input);
    static QImage histEqualize(const QImage &input);
    static QImage gaussianBlur(const QImage &input);
    static QImage laplacian(const QImage &input);

    // 分割
    static QImage adaptiveThreshold(const QImage &input);
    static QImage otsuThreshold(const QImage &input);

    // 特征提取
    static QImage morphOpen(const QImage &input);
    static QImage morphClose(const QImage &input);
    static QImage sobel(const QImage &input);
    static QImage prewitt(const QImage &input);

private:
    // 内部基础辅助函数
    static QImage toGray(const QImage &input);  //灰度转换
    static QImage dilate(const QImage &input);  //膨胀
    static QImage erode(const QImage &input);   //腐蚀
    static QImage convolve3x3(const QImage &input, const int kernel[3][3], int divisor = 1, int offset = 0);
};

#endif // PIXELPROCESSOR_H
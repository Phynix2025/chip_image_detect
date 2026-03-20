#include "core/PixelProcessor.h"
#include <QVector>
#include <cstdlib>
#include <qimage.h>
#include <qnamespace.h>
#include <qtypes.h>
#include <vector>
#include <cmath>
#include <algorithm>

bool PixelProcessor::saveImage(const QImage &image, const QString &filePath) {
    return image.save(filePath);
}

// 统一转换为 8 位灰度图，保证指针操作的安全和一致性
QImage PixelProcessor::toGray(const QImage &input) {
    if (input.format() == QImage::Format_Grayscale8) return input.copy();
    //虽然 input 是常引用，但是 convertToFormat 返回处理之后的拷贝
    return input.convertToFormat(QImage::Format_Grayscale8);
}

QImage PixelProcessor::linearStretch(const QImage &input) {
    //检查参数
    if(input.isNull()){

    }
    QImage gray = toGray(input);
    //默认3段线性拉伸,两点分三段,前后两段受到抑制，突出中间像素
    int x1 = 20,y1 = 10,x2 = 230,y2 = 240;
    //计算每个段的斜率
    float k1 = static_cast<float>(y1) / x1;
    float k2 = static_cast<float>(y2 - y1) / (x2 - x1);
    float k3 = static_cast<float>(255 - y2) / (255 - x2);
    //计算每个段的 y 方向偏移量
    int b1 = 0;
    int b2 = y1 - (k2 * x1);
    int b3 = y2 - (k3 * x2);
    //把 0-255 像素的对应值放在 查询表 里
    std::vector<int> vec(256);
    for(int i = 0; i < 256; ++ i){
        if(i <= x1){
            vec[i] = k1 * i + b1;
        }else if (i <= x2) {
            vec[i] = k2 * i + b2;
        }else {
            vec[i] = k3 * i + b3;
        }
    }
    //遍历像素，修改像素值
    int height = gray.height(),width = gray.width();
    int total = height * width;
    uchar *data = gray.bits();
    for(int i = 0; i < total; ++ i){
        data[i] = vec[data[i]];
    }
    return gray;
}

// 大津法 (Otsu)
QImage PixelProcessor::otsuThreshold(const QImage &input) {
    QImage gray = toGray(input);
    int total = gray.width() * gray.height();
    uchar *data = gray.bits();

    // 1. 计算直方图
    std::vector<int> hist(256);
    for (int i = 0; i < total; ++i){
        hist[data[i]]++;
    } 

    // 2. 遍历寻找最大类间方差
    float sum = 0;
    for (int i = 0; i < 256; ++i) sum += i * hist[i];

    //像素个数最多 10^6 ，最极限也就全部是 255，达到 10^8 数量级，没超过 int
    int sumB = 0,threshold = 0;
    int countB = 0;
    float maxV = 0.0;
    for(int i = 0; i < 256; ++ i){
        //累计背景像素个数
        countB += hist[i];
        if(countB == 0) {
            continue;
        }
        sumB += i * hist[i];
        //w0为背景像素点占整幅图像的比例
        float w0 = static_cast<float>(countB) / total;
        //u0为w0平均灰度
        float u0 = static_cast<float>(sumB) / countB;
        //w1为前景像素点占整幅图像的比例
        float w1 = static_cast<float>(total - countB) / total;
        //u1为w1平均灰度
        if(w1 == 0){
            continue;
        }
        float u1 = static_cast<float>(sum - sumB) / (total - countB);
        //u为整幅图像的平均灰度
        float u = static_cast<float>(sum) / total;
        //公式:g = w0*pow((u-u0),2) + w1*pow((u-u1),2)*/
        float g = w0 * std::pow((u - u0),2) + w1 * std::pow((u-u1), 2);
        if(g > maxV){
            maxV = g;
            threshold = i;
        }
    }
    //二值化
    for(int i =0; i < total; ++ i){
        if(data[i] < threshold){
            data[i] = 0;
        }else {
            data[i] = 255;
        }
    }
    return gray;
    // float sumB = 0, varMax = 0;
    // int wB = 0, wF = 0, threshold = 0;

    // for (int t = 0; t < 256; ++t) {
    //     wB += hist[t];
    //     if (wB == 0) continue;
    //     wF = total - wB;
    //     if (wF == 0) break;

    //     sumB += (float)(t * hist[t]);
    //     float mB = sumB / wB;
    //     float mF = (sum - sumB) / wF;

    //     float varBetween = (float)wB * (float)wF * (mB - mF) * (mB - mF);
    //     if (varBetween > varMax) {
    //         varMax = varBetween;
    //         threshold = t;
    //     }
    // }

    // // 3. 应用二值化
    // for (int i = 0; i < total; ++i) {
    //     data[i] = (data[i] >= threshold) ? 255 : 0;
    // }
    // return gray;
}

// --- 通用 3x3 卷积函数 (无边缘处理，最快) ---
QImage PixelProcessor::convolve3x3(const QImage &input, const int kernel[3][3], int divisor, int offset) {
    QImage src = toGray(input);
    QImage dst = src.copy();
    int w = src.width(), h = src.height();
    int stride = src.bytesPerLine();
    const uchar *sData = src.constBits();
    uchar *dData = dst.bits();

    for (int y = 1; y < h - 1; ++y) {
        for (int x = 1; x < w - 1; ++x) {
            int sum = 0;
            for (int ky = -1; ky <= 1; ++ky) {
                for (int kx = -1; kx <= 1; ++kx) {
                    sum += sData[(y + ky) * stride + (x + kx)] * kernel[ky + 1][kx + 1];
                }
            }
            int val = (sum / divisor) + offset;
            dData[y * stride + x] = static_cast<uchar>(std::clamp(val, 0, 255));
        }
    }
    return dst;
}

// Sobel 算子 
QImage PixelProcessor::sobel(const QImage &input) {
    //采用 x y 方向分开计算 
    //左侧系数是 -1，-2，-1 右侧系数：1，2，1 得到dy
    //上侧系数：-1，-2，-1；下侧系数：1，2，1 得到dx
    //最后 res = sqrt(dy * dy + dx * dx);
    //参数检查
    if(input.isNull()){

    }
    QImage gray = toGray(input);
    QImage res = gray.copy();
    //忽略边界
    int height = gray.height(),width = gray.width();
    for(int i = 1; i < height - 1; ++ i){
        const uchar *pre = gray.scanLine(i-1);
        const uchar *cur = gray.scanLine(i);
        const uchar *next = gray.scanLine(i+1);
        uchar *line = res.scanLine(i);
        for(int j = 1; j < width - 1; ++ j){
            int dx =next[j-1] + static_cast<int>(next[j]) * 2 + next[j+1] 
                    - (pre[j-1] + static_cast<int>(pre[j]) * 2 + pre[j+1]);
            int dy =pre[j+1] + static_cast<int>(cur[j+1]) * 2 + next[j+1]
                    - (pre[j-1] + static_cast<int>(cur[j-1]) * 2 + next[j-1]);
            //修改结果图
            int tmp = std::abs(dx) + std::abs(dy) ;
            tmp = std::min(std::max(0,tmp),255);
            line[j] = static_cast<uchar>(tmp);
        }
    }
    return res;

    // int Kx[3][3] = {{-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1}};
    // int Ky[3][3] = {{1, 2, 1}, {0, 0, 0}, {-1, -2, -1}};
    
    // QImage src = toGray(input);
    // QImage dst = src.copy();
    // int w = src.width(), h = src.height(), stride = src.bytesPerLine();
    // const uchar *sData = src.constBits();
    // uchar *dData = dst.bits();

    // for (int y = 1; y < h - 1; ++y) {
    //     for (int x = 1; x < w - 1; ++x) {
    //         int gx = 0, gy = 0;
    //         for (int ky = -1; ky <= 1; ++ky) {
    //             for (int kx = -1; kx <= 1; ++kx) {
    //                 int pixel = sData[(y + ky) * stride + (x + kx)];
    //                 gx += pixel * Kx[ky + 1][kx + 1];
    //                 gy += pixel * Ky[ky + 1][kx + 1];
    //             }
    //         }
    //         int val = std::abs(gx) + std::abs(gy); // 曼哈顿距离近似，速度比 sqrt 快
    //         dData[y * stride + x] = static_cast<uchar>(std::clamp(val, 0, 255));
    //     }
    // }
    // return dst;
}

// 高斯滤波
QImage PixelProcessor::gaussianBlur(const QImage &input) {
    //检查参数
    if(input.isNull()){

    }
    QImage gray = toGray(input);
    
    //自生成高斯滤波核，再进行卷积运算
    const float PI = 3.1415926,sigma = 0.8;
    int ksize = 3; //默认3 * 3 。这里不要改其他数值 !!!
    double coreV[ksize][ksize];
    int center = ksize / 2;
    double sum = 0.0; //这个滤波核总的权值
    for(int i = 0; i < ksize; ++ i){
        for(int j = 0; j < ksize; ++ j){
            double tmp = std::pow(i-center,2) + std::pow(j-center,2);
            tmp /= 2 * std::pow(sigma,2);
            tmp = std::exp(-tmp);
            tmp /= 2 * PI * std::pow(sigma, 2);
            coreV[i][j] = tmp;
            sum += tmp;
        }
    }
    //如果这里是 double 和 uchar 直接进行运算，然后结果强转成 uchar
    //这种方式不可取，所以要求化为正数形式再计算
    int x1 = 1,x2 = 2,x3 = 1;
    //利用高斯天生可拆性，行 和 列 运算基本类似
    int height = gray.height(),width = gray.width();
    //行 
    for(int i = 1 ; i < height - 1; ++ i){
        uchar *line = gray.scanLine(i);
        uchar prePix = line[0]; //保留前一个像素
        for(int j = 1; j < width -1 ; ++ j){
            uchar tmp = line[j];
            line[j] = (prePix * x1 + line[j] * x2 + line[j+1] * x3) / 4;
            prePix = tmp;
        }
    }
    QImage res{gray.copy()};
    for(int i = 1; i < height - 1; ++ i){
        const uchar *pre = gray.constScanLine(i-1);
        uchar *line = res.scanLine(i);
        const uchar *next = gray.constScanLine(i+1);
        for(int j = 1; j < width - 1; ++ j){
            line[j] = (pre[j] + line[j] * 2 + next[j]) / 4;
        }
    }
    
    return res;

    // int K[3][3] = {{1, 2, 1}, {2, 4, 2}, {1, 2, 1}};
    // return convolve3x3(input, K, 16);
}

//拉普拉斯锐化
//通过将增强后的细节(高频)叠加回原图来达到锐化效果，定义一个复合了锐化逻辑的卷积核
QImage PixelProcessor::laplacian(const QImage &input) {
    // 这是一个包含了原图叠加逻辑的锐化核 (基于含对角线的-8拉普拉斯核推导)
    // 其元素和为1，能保持原图亮度，同时增强细节。
    // int SharpeningK[3][3] = {
    //     {-1, -1, -1},
    //     {-1,  9, -1},
    //     {-1, -1, -1}
    // };
    // // 复用卷积函数
    // return convolve3x3(input, SharpeningK);

    //法二：n自实现，降低时间复杂度
    if(input.isNull()){

    }
    QImage gray = toGray(input);
    //防止输入图像是 RGB，多做一步处理
    QImage res = gray.copy();
    int height = gray.height(),width = gray.width();
    for(int i = 1; i < height - 1; ++ i){
        const uchar *pre = gray.constScanLine(i-1);
        const uchar *cur = gray.constScanLine(i);
        const uchar *next = gray.constScanLine(i+1);
        uchar *line = res.scanLine(i);
        for (int j = 1; j < width - 1; ++ j) {
            int tmp = cur[j] * 9 - pre[j] - pre[j-1] - pre[j+1]
                    - cur[j-1] - cur[j+1] - next[j-1] - next[j] - next[j+1];
            tmp = std::clamp(tmp,0,255);
            line[j] = static_cast<uchar>(tmp);
        }
    }
    return res;
}

// 腐蚀：取当前像素点为中心的十字架最小值替换当前像素
QImage PixelProcessor::erode(const QImage &input) {
    QImage src = toGray(input);
    QImage dst = src.copy();
    int w = src.width(), h = src.height(), stride = src.bytesPerLine();
    uchar *data = dst.bits();
    const uchar *ori = src.constBits();
    for(int y = 1; y < h -1 ; ++ y){
        for(int x = 1; x < w - 1; ++ x){
            uchar minPix = 255;
            int curPos = y * stride + x;
            minPix = std::min(minPix,ori[curPos - stride]);
            minPix = std::min(minPix,ori[curPos + stride]);
            minPix = std::min(minPix,ori[curPos - 1]);
            minPix = std::min(minPix,ori[curPos + 1]);
            minPix = std::min(minPix,ori[curPos]);
            data[curPos] = minPix;
        }
    }
    return dst;
    // for (int y = 1; y < h - 1; ++y) {
    //     for (int x = 1; x < w - 1; ++x) {
    //         uchar minVal = 255;
    //         for (int ky = -1; ky <= 1; ++ky)
    //             for (int kx = -1; kx <= 1; ++kx)
    //                 minVal = std::min(minVal, src.constBits()[(y + ky) * stride + (x + kx)]);
    //         dst.bits()[y * stride + x] = minVal;
    //     }
    // }
    // return dst;
}
//膨胀 取十字架像素最大值作为中心点像素
QImage PixelProcessor::dilate(const QImage &input) {
    QImage src = toGray(input);
    QImage dst = src.copy();
    int w = src.width(), h = src.height(), stride = src.bytesPerLine();
    uchar *data = dst.bits();
    const uchar *ori = src.constBits();
    for(int y = 1; y < h -1 ; ++ y){
        for(int x = 1; x < w - 1; ++ x){
            uchar minPix = 0;
            int curPos = y * stride + x;
            minPix = std::max(minPix,ori[curPos - stride]);
            minPix = std::max(minPix,ori[curPos + stride]);
            minPix = std::max(minPix,ori[curPos - 1]);
            minPix = std::max(minPix,ori[curPos + 1]);
            minPix = std::max(minPix,ori[curPos]);
            data[curPos] = minPix;
        }
    }
    return dst;
    // for (int y = 1; y < h - 1; ++y) {
    //     for (int x = 1; x < w - 1; ++x) {
    //         uchar maxVal = 0;
    //         for (int ky = -1; ky <= 1; ++ky)
    //             for (int kx = -1; kx <= 1; ++kx)
    //                 maxVal = std::max(maxVal, src.constBits()[(y + ky) * stride + (x + kx)]);
    //         dst.bits()[y * stride + x] = maxVal;
    //     }
    // }
    // return dst;
}

// 开运算：先腐蚀，后膨胀 (消除小白点噪声)
QImage PixelProcessor::morphOpen(const QImage &input) { 
    return dilate(erode(input)); 
}

// 闭运算：先膨胀，后腐蚀 (填补小黑洞)
QImage PixelProcessor::morphClose(const QImage &input) { 
    return erode(dilate(input)); 
}

// 直方图均衡化 ---
// 原理：计算概率密度函数(PDF)和累积分布函数(CDF)，将灰度值映射到更均匀的分布
QImage PixelProcessor::histEqualize(const QImage &input) {
    QImage gray = toGray(input);
    int total = gray.width() * gray.height();
    uchar *data = gray.bits();

    //统计直方图
    std::vector<int> hist(256);
    for(int i = 0; i < total; ++ i){
        hist[data[i]] ++;
    }
    //累计分布
    int lowPix = hist[0]; //所拥有的最小像素个数
    for(int i = 1; i < 256; ++ i){
        hist[i] += hist[i-1];
        if(hist[i] > 0 && lowPix == 0){
            lowPix = hist[0];
        }
    }

    //计算 0-255 像素匹配,灰度映射
    std::vector<uchar> mp(256);
    for(int i = 0; i < 256; ++ i){
        float tmp = (hist[i] - lowPix) * 255.0f / (total - lowPix);
        tmp = std::clamp(tmp,0.0f,255.0f);
        mp[i] = static_cast<uchar>(tmp);
    }
    for(int i = 0; i < total; ++ i){
        data[i] = mp[data[i]];
    }

    return gray;
    // // 1. 统计直方图
    // int hist[256] = {0};
    // for (int i = 0; i < total; ++i) {
    //     hist[data[i]]++;
    // }

    // // 2. 计算累积分布函数 (CDF)
    // int cdf[256] = {0};
    // cdf[0] = hist[0];
    // int cdf_min = cdf[0];
    // for (int i = 1; i < 256; ++i) {
    //     cdf[i] = cdf[i - 1] + hist[i];
    //     if (hist[i] != 0 && cdf_min == 0) {
    //         cdf_min = cdf[i]; // 找到第一个非零的 CDF 值
    //     }
    // }

    // // 3. 构建映射表并应用映射
    // uchar map[256];
    // for (int i = 0; i < 256; ++i) {
    //     // 映射公式: round((cdf[v] - cdf_min) / (total - cdf_min) * 255)
    //     float mappedValue = (cdf[i] - cdf_min) * 255.0f / (total - cdf_min);
    //     map[i] = static_cast<uchar>(std::clamp(mappedValue, 0.0f, 255.0f));
    // }

    // for (int i = 0; i < total; ++i) {
    //     data[i] = map[data[i]];
    // }

    // return gray;
}

// 自适应阈值 (使用积分图的局部均值)
// 原理：对比每个像素与其周围 15x15 邻域的均值，能够有效克服光照不均的影响
QImage PixelProcessor::adaptiveThreshold(const QImage &input) {
    QImage gray = toGray(input);
    int w = gray.width(), h = gray.height();
    QImage dst = gray.copy();
    const uchar *sData = gray.constBits();
    uchar *dData = dst.bits();
    int stride = gray.bytesPerLine();

    // 1. 构建积分图 (长宽各加1以处理边界，使用一维 vector 模拟二维矩阵)
    std::vector<int> integral((w + 1) * (h + 1), 0);
    for (int y = 0; y < h; ++y) {
        int sum = 0;
        for (int x = 0; x < w; ++x) {
            sum += sData[y * stride + x];
            // 积分图公式：当前行累加和 + 上方相邻的积分值
            integral[(y + 1) * (w + 1) + (x + 1)] = integral[y * (w + 1) + (x + 1)] + sum;
        }
    }

    // 2. 利用积分图计算局部均值并进行二值化
    int S = 15;      // 局部窗口大小
    int C = 5;       // 补偿常数 
    int radius = S / 2;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            // 确定局部窗口的有效边界
            int x1 = std::max(0, x - radius);
            int y1 = std::max(0, y - radius);
            int x2 = std::min(w - 1, x + radius);
            int y2 = std::min(h - 1, y + radius);

            int count = (x2 - x1 + 1) * (y2 - y1 + 1);
            
            // 积分图 O(1) 核心查询公式：右下 - 左下 - 右上 + 左上
            int sum = integral[(y2 + 1) * (w + 1) + (x2 + 1)] 
                    - integral[(y1) * (w + 1) + (x2 + 1)] 
                    - integral[(y2 + 1) * (w + 1) + (x1)] 
                    + integral[(y1) * (w + 1) + (x1)];

            int mean = sum / count;
            
            // 如果像素值大于 (局部均值 - 补偿值)，则赋为白(255)，否则为黑(0)
            dData[y * stride + x] = (sData[y * stride + x] > (mean - C)) ? 255 : 0;
        }
    }
    return dst;
}

// Prewitt 算子
// 原理：与 Sobel 类似，但卷积核没有中心权重(全为1)，对噪声敏感度稍低
QImage PixelProcessor::prewitt(const QImage &input) {
    // Prewitt 的水平和垂直核
    int Kx[3][3] = {{-1, 0, 1}, {-1, 0, 1}, {-1, 0, 1}};
    int Ky[3][3] = {{ 1,  1,  1}, { 0,  0,  0}, {-1, -1, -1}};
    
    QImage src = toGray(input);
    QImage dst = src.copy();
    int w = src.width(), h = src.height(), stride = src.bytesPerLine();
    const uchar *sData = src.constBits();
    uchar *dData = dst.bits();

    for (int y = 1; y < h - 1; ++y) {
        for (int x = 1; x < w - 1; ++x) {
            int gx = 0, gy = 0;
            for (int ky = -1; ky <= 1; ++ky) {
                for (int kx = -1; kx <= 1; ++kx) {
                    int pixel = sData[(y + ky) * stride + (x + kx)];
                    gx += pixel * Kx[ky + 1][kx + 1];
                    gy += pixel * Ky[ky + 1][kx + 1];
                }
            }
            // 使用绝对值和近似梯度幅值
            int val = std::abs(gx) + std::abs(gy); 
            dData[y * stride + x] = static_cast<uchar>(std::clamp(val, 0, 255));
        }
    }
    return dst;
}
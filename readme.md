## 日志
### 3.16
1. 首先保证自己的qtcreator能创建项目，然后再vscode配置qt环境只需要qt configure插件，设置qt dir和qt kits dir 即可。注意qt的CMakeLists.txt qt_add_excutable需要把头文件，ui文件一起加进去。修改vscode的C++编译检查插件，让他从CMake那获取参考头文件路径
### 3.17
1. 自己写写基础图像处理算法
    [分段线性拉伸](https://www.cnblogs.com/yaoguang-willZ/p/14737717.html)
### 3.18
    [大津法](https://www.cnblogs.com/ranjiewen/p/6385564.html)
    [Sobel](https://www.cnblogs.com/IllidanStormrage/p/16206945.html)
    [高斯滤波](https://www.cnblogs.com/wangguchangqing/p/6407717.html)
### 3.19
    [拉普拉斯](https://juejin.cn/post/7362757977734823987)
    ! 注意：当心 input 是 32 位深度图像
    腐蚀：十字架最小值替换中间像素
    膨胀：十字架最大值替换1中间像素
    [积分图](https://www.cnblogs.com/weijiakai/p/15495684.html)
    思路：现在应该用最简单最笨的方法匹配模板，然后将识别出的缺陷原地画白色矩形进行显示 （已完成）
    需要修改 ui 逻辑：将原图和模板图传递给 异步线程 （已完成）
    线程资源保留原图和模板图，线程进行计算，返回处理后的图片和结论 （已完成框架）
### 3.20
    1. 最笨的逐像素模板匹配，对于 bottom 图片匹配失误！！！
    2. 可能需要大改代码逻辑，老师要求界面直观能体现缺陷检测过程，体现工作量，代码量 3k 行必不可少
    3. 即使是采用 跨步粗匹配 再 细匹配 效率也很慢，再想想。



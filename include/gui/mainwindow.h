#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "core/ProcessingThread.h"

#include <QMainWindow>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QLabel>
#include <QPushButton>
#include <QStringList>
#include <QStack>
#include <QImage>
#include <qimage.h>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void showPreviousImage(); //上一张
    void showNextImage(); //下一张
    
    //工具栏与撤销操作槽函数
    void onActionOpen();
    void onActionSaveAs();
    void onActionDetect();
    void onUndo();
    //处理接收子线程处理完毕的信号
    void onProcessingFinished(QImage resultImage,QString message);
private:
    void setupUi();
    void loadImages();
    void updateImageDisplay();

    //内部辅助函数
    void applyProcessedImage(const QImage &newImage,const QString &resultMsg = "-");
    void refreshGraphicsView();
    void startProcessingTask(ProcessingThread::TaskType taskType);


    // 核心显示组件：为了后续的缩放和画框，必须使用 Graphics 框架
    QGraphicsView *imageView;
    QGraphicsScene *imageScene;
    QGraphicsPixmapItem *pixmapItem;

    // 按钮
    QPushButton *btnPrev;
    QPushButton *btnNext;
    QPushButton *btnUndo;

    // 图像信息标签
    QLabel *lblResolution;
    QLabel *lblBitDepth;
    QLabel *lblResult;

    // 目录与索引管理
    QString currentImageDir;
    QStringList imageFiles;
    int currentIndex;

    //图像状态和撤销栈
    QImage currentImage; //保存当前显示得内存图像
    QStack<QImage> undoStack; //历史操作栈

    QImage oriImage;//保留当期的原始图像
    QImage standard; //标准图像

};

#endif // MAINWINDOW_H
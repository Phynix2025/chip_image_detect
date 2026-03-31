#include "gui/mainwindow.h"
#include "core/PixelProcessor.h"
#include "core/ProcessingThread.h"

#include <QMessageBox>
#include <QToolBar>
#include <QAction>
#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QWheelEvent>
#include <QFileDialog>
#include <QToolButton>
#include <QMenu>
#include <qaction.h>
#include <qcoreapplication.h>
#include <qdir.h>
#include <qfiledialog.h>
#include <qglobal.h>
#include <qimage.h>
#include <qmenu.h>
#include <qmessagebox.h>
#include <qobject.h>
#include <qtoolbutton.h>
#include <utility>
#include <QDebug>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), currentIndex(-1), pixmapItem(nullptr) {
    
    setWindowTitle("芯片图像缺陷检测工具");
    resize(1000, 600);

    setupUi();
    //loadImages();
}

MainWindow::~MainWindow() {
    // Qt 的对象树机制会自动清理挂载在 this 上的子组件
}

void MainWindow::setupUi() {
    QToolBar *toolbar = addToolBar("主工具栏");
    // 打开缺陷图片所在的文件夹
    QAction *actOpen = toolbar->addAction("打开");
    connect(actOpen,&QAction::triggered,this,&MainWindow::onActionOpen);

    // --- 另存为 ---
    QAction *actSaveAs = toolbar->addAction("另存为");
    connect(actSaveAs, &QAction::triggered, this, &MainWindow::onActionSaveAs);

    // --- 预处理菜单 ---
    QToolButton *btnPre = new QToolButton(this);
    btnPre->setText("预处理");
    btnPre->setPopupMode(QToolButton::InstantPopup);
    QMenu *menuPre = new QMenu(this);
    menuPre->addAction("线性拉伸", this, [this](){ startProcessingTask(ProcessingThread::LinearStretch); });
    menuPre->addAction("直方图均衡化", this, [this](){ startProcessingTask(ProcessingThread::HistEq); });
    menuPre->addAction("高斯滤波", this, [this](){ startProcessingTask(ProcessingThread::Gaussian); });
    menuPre->addAction("拉普拉斯锐化", this, [this](){ startProcessingTask(ProcessingThread::Laplacian); });
    // 加入双边滤波
    menuPre->addAction("双边滤波",this,[this]() {startProcessingTask(ProcessingThread::TwoWayFilter); });
    // 图像增强
    menuPre->addAction("Retinex 增强",this,[this]() {startProcessingTask(ProcessingThread::Retinex); });
    btnPre->setMenu(menuPre);
    toolbar->addWidget(btnPre);

    // --- 分割菜单 ---
    QToolButton *btnSeg = new QToolButton(this);
    btnSeg->setText("分割");
    btnSeg->setPopupMode(QToolButton::InstantPopup);
    QMenu *menuSeg = new QMenu(this);
    menuSeg->addAction("自适应阈值", this, [this](){ startProcessingTask(ProcessingThread::AdaptiveThresh); });
    menuSeg->addAction("大津法 (Otsu)", this, [this](){ startProcessingTask(ProcessingThread::Otsu); });
    btnSeg->setMenu(menuSeg);
    toolbar->addWidget(btnSeg);

    // --- 特征提取菜单 ---
    QToolButton *btnExt = new QToolButton(this);
    btnExt->setText("特征提取");
    btnExt->setPopupMode(QToolButton::InstantPopup);
    QMenu *menuExt = new QMenu(this);
    menuExt->addAction("形态学开运算", this, [this](){ startProcessingTask(ProcessingThread::MorphOpen); });
    menuExt->addAction("形态学闭运算", this, [this](){ startProcessingTask(ProcessingThread::MorphClose); });
    menuExt->addAction("Sobel 算子", this, [this](){ startProcessingTask(ProcessingThread::Sobel); });
    menuExt->addAction("Prewitt 算子", this, [this](){ startProcessingTask(ProcessingThread::Prewitt); });
    btnExt->setMenu(menuExt);
    toolbar->addWidget(btnExt);

    // --- 缺陷检测菜单 ---
    QToolButton *btnDet = new QToolButton(this);
    btnDet->setText("缺陷检测");
    btnDet->setPopupMode(QToolButton::InstantPopup);
    QMenu *menuDet = new QMenu(this);
    menuDet->addAction("模板匹配",this,[this](){startProcessingTask(ProcessingThread::TemplateMatch); });
    menuDet->addAction("图像差分",this,[this]() {startProcessingTask(ProcessingThread::ImageDiff); });
    menuDet->addAction("阈值分割",this,[this]() {startProcessingTask(ProcessingThread::ThreshSeg); });
    menuDet->addAction("断点连接",this,[this]() {startProcessingTask(ProcessingThread::PointLink);} );
    menuDet->addAction("缺陷分析",this,[this]() {startProcessingTask(ProcessingThread::DefectAnalysis);} );
    btnDet->setMenu(menuDet);
    toolbar->addWidget(btnDet);

    // ====================
    // 2. 核心布局 (3:2 分配)
    // ====================
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);

    // --- 左侧：图像显示与控制区域 ---
    QWidget *leftWidget = new QWidget(this);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftWidget);

    // 使用 QGraphicsView 替代 QLabel
    imageView = new QGraphicsView(this);
    imageScene = new QGraphicsScene(this);
    imageView->setScene(imageScene);
    imageView->setStyleSheet("background-color: #2e2e2e; border: none;");
    // 隐藏滚动条，后续通过滚轮控制缩放时体验更好
    imageView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    imageView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // 【新增】缩放与交互配置
    imageView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse); // 关键：以鼠标光标所在位置为中心进行缩放
    imageView->setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    imageView->setDragMode(QGraphicsView::ScrollHandDrag); // 允许鼠标左键按住拖拽平移图像

    // 【新增】给 imageView 的视口安装事件过滤器，由 MainWindow 来拦截事件
    imageView->viewport()->installEventFilter(this);

    // 左右切换按钮
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnPrev = new QPushButton("<- 上一张", this);
    btnNext = new QPushButton("下一张 ->", this);
    btnLayout->addWidget(btnPrev);
    btnLayout->addStretch();
    btnLayout->addWidget(btnNext);

    connect(btnPrev, &QPushButton::clicked, this, &MainWindow::showPreviousImage);
    connect(btnNext, &QPushButton::clicked, this, &MainWindow::showNextImage);

    leftLayout->addWidget(imageView);
    leftLayout->addLayout(btnLayout);

    // --- 右侧：信息与操作区域 ---
    QWidget *rightWidget = new QWidget(this);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightWidget);

    btnUndo = new QPushButton("撤销", this);
    btnUndo->setEnabled(false); //初始栈为空，不可以撤销
    connect(btnUndo,&QPushButton::clicked,this,&MainWindow::onUndo);
    rightLayout->addWidget(btnUndo);

    QFormLayout *infoLayout = new QFormLayout();
    lblResolution = new QLabel("-", this);
    lblBitDepth = new QLabel("-", this);
    lblResult = new QLabel("-", this);
    
    QFont boldFont = lblResult->font();
    boldFont.setBold(true);
    lblResult->setFont(boldFont);
    lblResult->setStyleSheet("color: #e74c3c;");

    infoLayout->addRow("分辨率:", lblResolution);
    infoLayout->addRow("位深度:", lblBitDepth);
    infoLayout->addRow("检测结果:", lblResult);

    rightLayout->addLayout(infoLayout);
    rightLayout->addStretch();

    // 将左右区域加入主布局并设置 3:2 比例
    mainLayout->addWidget(leftWidget, 3);
    mainLayout->addWidget(rightWidget, 2);
}

void MainWindow::onActionOpen(){
    //思路：只需要放入相对路径，然后得到绝对路径传递给 loadImages （类成员 currentDir）
    /*QDir dir(QCoreApplication::applicationDirPath());
    //向上2级
    dir.cdUp();
    dir.cdUp();
    QString projectPath = dir.absolutePath();
    return projectPath;
    QString fileName = QFileDialog::getOpenFileName(this, tr("打开文件"),
                                                    MainWindow::get_path(),
                                                    "*.png *.xpm *.jpg *.bmp");
    */
    //打开缺陷图片所在的文件夹
    QDir dir{QCoreApplication::applicationDirPath()};
    dir.cdUp();
    //采用 QDir::fromNativeSeparators() 根据操作系统返回合适的分隔符，如：“\\” - windows
    QString dirPath = dir.absolutePath() + QDir::fromNativeSeparators("/data") ;
    //提示用户打开缺陷图片所在的文件夹
    QString dirName = QFileDialog::getExistingDirectory(this,"打开文件夹",dirPath);
    //得到目录
    currentImageDir = std::move(dirName);
    //获取标准图像
    QDir dirStd(currentImageDir); //标准图像路径
    dirStd.cdUp();
    if(!dirStd.cd("standard")){
        QMessageBox::warning(this,"错误","没有 standard 文件夹");
        return;
    }
    QStringList nameFilters;  //过滤器，只要图片类型文件
    nameFilters << "*.png" << "*.jpg" << "*.jpeg" << "*.bmp" << "*.tif";
    imageFiles = dirStd.entryList(nameFilters, QDir::Files | QDir::NoSymLinks, QDir::Name);

    if(imageFiles.isEmpty()){
        QMessageBox::warning(this,"错误","standard 目录下没有图片");
    }
    //标准图像路径 拼接
    QString imgPath = dirStd.absolutePath();
    imgPath += QDir::fromNativeSeparators("/") + imageFiles[0];
    
    //读取 作为模板图
    standard = QImage(imgPath);

    loadImages(); //加载缺陷图相关信息
}

void MainWindow::loadImages() {
    // 使用 "/../data" 退回上一级目录，并用 cleanPath 整理路径
    //currentImageDir = QDir::cleanPath(QDir::currentPath() + "/../data");
    
    QDir dir(currentImageDir);

    if (dir.exists()) {
        QStringList nameFilters;
        nameFilters << "*.png" << "*.jpg" << "*.jpeg" << "*.bmp" << "*.tif";
        imageFiles = dir.entryList(nameFilters, QDir::Files | QDir::NoSymLinks, QDir::Name);
    }

    if (!imageFiles.isEmpty()) {
        currentIndex = 0;
        updateImageDisplay();
    } else {
        // 更新了提示信息，方便在界面上看到去哪个绝对路径找图片了
        imageScene->addText(QString("未在 \n%1 \n找到图片").arg(currentImageDir))->setDefaultTextColor(Qt::white);
        btnPrev->setEnabled(false);
        btnNext->setEnabled(false);
    }
}

void MainWindow::updateImageDisplay() {
    if (currentIndex < 0 || currentIndex >= imageFiles.size()) return;

    QString imgPath = currentImageDir + "/" + imageFiles[currentIndex];
    
    //读取 QImage 作为基准数据
    currentImage = QImage(imgPath);

    if(!currentImage.isNull()){
        //保存原始图像
        oriImage = currentImage.copy();
        //切换图片，清空栈，重置 ui
        undoStack.clear();
        btnUndo->setEnabled(false);
        lblResult->setText("等待检测...");
        lblResult->setStyleSheet("color: #e74c3c");

        //刷新视图
        refreshGraphicsView();
    }

    // 更新按钮状态
    btnPrev->setEnabled(currentIndex > 0);
    btnNext->setEnabled(currentIndex < imageFiles.size() - 1);
}

//将 QImage 渲染到界面的辅助函数
void MainWindow::refreshGraphicsView(){
    if(currentImage.isNull()) return;

    imageScene->clear();

    pixmapItem = imageScene->addPixmap(QPixmap::fromImage(currentImage));
    imageView->fitInView(pixmapItem,Qt::KeepAspectRatio);

    lblResolution->setText(QString("%1 x %2").arg(currentImage.width()).arg(currentImage.height()));
    lblBitDepth->setText(QString::number(currentImage.depth()));
}

//操作状态管理
void MainWindow::applyProcessedImage(const QImage &newImage, const QString &resultMsg) {
    if (newImage.isNull()) return;

    // 1. 执行操作前，将当前状态压入栈中
    undoStack.push(currentImage);
    
    // 2. 更新当前状态为处理后的新图像
    currentImage = newImage;
    
    // 3. 激活撤销按钮并刷新显示
    btnUndo->setEnabled(true);
    refreshGraphicsView();
    
    if (resultMsg != "-") {
        lblResult->setText(resultMsg);
    }
}

// ==========================================
// 业务逻辑槽函数实现 (异步线程调用)
// ==========================================

void MainWindow::startProcessingTask(ProcessingThread::TaskType taskType){
    if(currentImage.isNull()) return;

    //更新 ui 状态，提示用户正在处理
    lblResult->setText("处理中...");
    lblResult->setStyleSheet("color: #3498db;");

    //暂时禁用主窗口，防止在处理期间反复被点击
    //this->setEnabled(false);

    //创建子线程
    ProcessingThread *thread = new ProcessingThread(taskType,currentImage,oriImage,standard,this);

    //连接信号与槽
    //线程发送 resultReady ，由主线程的 onProcessingFinished 接受
    connect(thread,&ProcessingThread::resultReady,this,&MainWindow::onProcessingFinished);

    //线程执行完毕后自动销毁线程对象，防止内存泄漏
    connect(thread,&QThread::finished,thread,&QObject::deleteLater);

    //启动线程
    thread->start();
}

//接收子线程对的结果
void MainWindow::onProcessingFinished(QImage resultImage,QString message){
    //恢复窗口交互
    //this->setEnabled(true);

    if(message.contains("异常") || message.contains("缺陷")){
        lblResult->setStyleSheet("color: #f39c12");
    }else{
        lblResult->setStyleSheet("color: #2ecc71");
    }

    //入栈，刷新界面
    applyProcessedImage(resultImage,message);
}


void MainWindow::onActionSaveAs() {
    if (currentImage.isNull()) return;
    QString savePath = QFileDialog::getSaveFileName(this, "另存为", currentImageDir, "Images (*.png *.jpg *.bmp)");
    if (!savePath.isEmpty()) {
        if(PixelProcessor::saveImage(currentImage, savePath)) {
            // 保存成功提示
            QMessageBox::information(this,"提示","保存成功！");
        }
    }
}


void MainWindow::onUndo() {
    if (!undoStack.isEmpty()) {
        // 弹出上一个状态并覆盖当前状态
        currentImage = undoStack.pop();
        
        // 当栈弹空时，禁用撤销按钮
        btnUndo->setEnabled(!undoStack.isEmpty());
        
        lblResult->setText("已撤销操作");
        lblResult->setStyleSheet("color: #e74c3c;");
        
        refreshGraphicsView();
    }
}

void MainWindow::showPreviousImage() {
    if (currentIndex > 0) {
        currentIndex--;
        updateImageDisplay();
    }
}

void MainWindow::showNextImage() {
    if (currentIndex < imageFiles.size() - 1) {
        currentIndex++;
        updateImageDisplay();
    }
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    // 必须先调用父类的 resizeEvent，确保正常的布局计算完成
    QMainWindow::resizeEvent(event);
    
    // 如果当前场景里已经加载了图片图元，就让它自适应当前的 View 大小
    if (pixmapItem) {
        imageView->fitInView(pixmapItem, Qt::KeepAspectRatio);
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    // 拦截 imageView 视口的滚轮事件
    if (watched == imageView->viewport() && event->type() == QEvent::Wheel) {
        QWheelEvent *wheelEvent = static_cast<QWheelEvent*>(event);
        
        // angleDelta().y() 大于 0 表示滚轮向前推（放大），小于 0 表示向后滚（缩小）
        int angle = wheelEvent->angleDelta().y();
        if (angle != 0) {
            // 设定每次滚动的缩放步长，1.15 倍是一个视觉上比较舒适的比例
            qreal factor = (angle > 0) ? 1.15 : (1.0 / 1.15);
            imageView->scale(factor, factor);
        }
        
        // 返回 true 表示这个事件我们已经处理完毕，不需要 Qt 再去做默认的滚动条处理
        return true; 
    }
    
    // 其他不关心的事件，统统交还给基类按默认逻辑处理
    return QMainWindow::eventFilter(watched, event);
}
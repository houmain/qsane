#include "MainWindow.h"
#include "./ui_MainWindow.h"
#include "WorkerThread.h"
#include "CropRect.h"
#include "GraphicsImageItem.h"
#include <QSettings>
#include <QFileDialog>
#include <QMessageBox>
#include <QMouseEvent>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , mWorkerThread(new WorkerThread(this))
    , mSettings(new QSettings(this))
{
    ui->setupUi(this);
    ui->retranslateUi(this);

    setWindowTitle(tr("Photo Scanner"));

    auto icon = QIcon(":icons/qsane.png");
    setWindowIcon(icon);

    mScene = ui->pageView->scene();
    mPreviewItem = new GraphicsImageItem();
    mScene->addItem(mPreviewItem);
    mImageItem = new GraphicsImageItem();
    mScene->addItem(mImageItem);
    mCropRect = new CropRect();

    ui->groupBoxProperties->setVisible(false);
    ui->widgetIndex->setEnabled(false);

    connect(ui->checkBoxAdvanced, &QCheckBox::toggled,
        ui->propertyBrowser, &DevicePropertyBrowser::setShowAdvanced);
    connect(ui->buttonSave, &QPushButton::clicked, this, &MainWindow::save);
    connect(ui->buttonRefreshDevices, &QPushButton::clicked,
        this, &MainWindow::refreshDevices);
    connect(ui->buttonBrowse, &QPushButton::clicked,
        this, &MainWindow::browse);
    connect(ui->checkBoxIndexed, &QCheckBox::toggled,
        ui->widgetIndex, &QWidget::setEnabled);
    connect(ui->comboFolder, &QComboBox::currentTextChanged,
        this, &MainWindow::enableSaveButton);
    connect(ui->title, &QLineEdit::textChanged,
        this, &MainWindow::enableSaveButton);
    connect(ui->pageView, &PageView::mousePressed,
        this, &MainWindow::handlePageViewMousePressed);

    connect(ui->comboDevice, &QComboBox::currentIndexChanged,
        this, &MainWindow::handleDeviceIndexChanged);
    connect(mWorkerThread, &WorkerThread::scanStarted,
        this, &MainWindow::handleScanStarted);
    connect(mWorkerThread, &WorkerThread::scanComplete,
        this, &MainWindow::handleScanComplete);
    connect(mWorkerThread, &WorkerThread::scanLineScanned,
        this, &MainWindow::handleScanLineScanned);

    readSettings();
    enableSaveButton();
    QTimer::singleShot(500, this, &MainWindow::refreshDevices);
}

MainWindow::~MainWindow()
{
    delete mWorkerThread;
    delete ui;
    closeScanner();
    shutdownSane();
}

void MainWindow::readSettings()
{
    auto &s = *mSettings;
    s.beginGroup("General");

    if (!restoreGeometry(s.value("geometry").toByteArray()))
        setGeometry(100, 100, 800, 600);
    else if (s.value("maximized").toBool())
        showMaximized();

    ui->title->setText(s.value("title").toString());
    ui->indexSeparator->setText(s.value("indexSeparator", " ").toString());
    ui->checkBoxIndexed->setChecked(s.value("indexed").toBool());
    const auto folders = s.value("recentFolders", QStringList()).toStringList();
    for (const auto &path : folders)
        addFolder(path);
}

void MainWindow::writeSettings()
{
    auto &s = *mSettings;

    if (!isMaximized())
        s.setValue("geometry", saveGeometry());
    if (!isFullScreen())
        s.setValue("maximized", isMaximized());
    s.setValue("state", saveState());

    s.setValue("resolution", ui->comboResolution->currentText().toInt());
    s.setValue("title", ui->title->text());
    s.setValue("indexSeparator", ui->indexSeparator->text());
    s.setValue("indexed", ui->checkBoxIndexed->isChecked());
    auto folders = QStringList();
    for (auto i = ui->comboFolder->count() - 1; i >= 0; --i)
        folders << ui->comboFolder->itemData(i).toString();
    s.setValue("recentFolders", folders);

    s.endGroup();
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_F12)
        togglePropertyBrowser();

    QMainWindow::keyPressEvent(event);
}

void MainWindow::togglePropertyBrowser()
{
    const auto show = !ui->groupBoxProperties->isVisible();
    ui->groupBoxProperties->setVisible(show);
    ui->propertyBrowser->setScanner(show ? mScanner.data() : nullptr);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    event->accept();
    writeSettings();
}

void MainWindow::refreshDevices()
{
    closeScanner();

    const auto devices = enumerateDevices();
    ui->comboDevice->clear();
    for (const auto &device : devices)
        ui->comboDevice->addItem(device.vendor + " " + device.model, device.name);
}

void MainWindow::handleDeviceIndexChanged(int index)
{
    const auto deviceName = ui->comboDevice->itemData(index).toString();
    if (!deviceName.isEmpty())
        openScanner(deviceName);
}

void MainWindow::openScanner(const QString &deviceName)
{
    closeScanner();

    mScanner.reset(new Scanner());
    mScanner->open(deviceName);

    refreshControls();
    ui->comboResolution->setCurrentText(mSettings->value("resolution").toString());
    enableScannerBindings();
}

void MainWindow::closeScanner()
{
    disableScannerBindings();
    mScanner.reset();
}

void MainWindow::refreshControls()
{
    const auto resolutions = mScanner->getUniformResolutions();
    const auto resolution = mScanner->getResolution();
    ui->comboResolution->clear();
    for (auto resolution : resolutions)
        ui->comboResolution->addItem(QString::number(resolution), resolution);
    ui->comboResolution->setCurrentText(QString::number(
        std::min(resolution.x(), resolution.y())));

    const auto maximumBounds = mScanner->getMaximumBounds();
    ui->pageView->setBounds(maximumBounds);
    mCropRect->setBounds(mScanner->getBounds());
    mCropRect->setMaximumBounds(maximumBounds);
}

void MainWindow::enableScannerBindings()
{
    auto &c = mScannerBindings;
    if (!c.isEmpty())
        return;

    c += connect(ui->buttonPreview, &QPushButton::clicked, this, &MainWindow::preview);
    c += connect(ui->buttonScan, &QPushButton::clicked, this, &MainWindow::scan);
    c += connect(ui->comboResolution, &QComboBox::currentIndexChanged,
        this, &MainWindow::handleResolutionChanged);
    c += connect(mCropRect, &CropRect::transforming,
        this, &MainWindow::handleCropRectTransforming);
    c += connect(ui->propertyBrowser, &DevicePropertyBrowser::valueChanged,
        this, &MainWindow::refreshControls);

    ui->propertyBrowser->setEnabled(true);
    handleResolutionChanged(ui->comboResolution->currentIndex());
    handleCropRectTransforming(mCropRect->bounds());
}

void MainWindow::disableScannerBindings()
{
    for (auto &c : qAsConst(mScannerBindings))
        disconnect(c);
    mScannerBindings.clear();
    ui->propertyBrowser->setEnabled(false);
}

void MainWindow::handleResolutionChanged(int index)
{
    auto resolution = ui->comboResolution->itemData(index).toDouble();
    mScanner->setResolution({ resolution, resolution });
    ui->propertyBrowser->refreshProperties();
}

void MainWindow::handlePageViewMousePressed(const QPointF &position)
{
    mScene->addItem(mCropRect);
    mCropRect->setPos(position);
    mCropRect->startResize();
}

void MainWindow::handleCropRectTransforming(const QRectF &bounds)
{
    mScanner->setBounds(bounds);
    ui->propertyBrowser->refreshProperties();
}

void MainWindow::preview()
{
    disableScannerBindings();
    mImageItem->clear();
    mScanningItem = mPreviewItem;
    mWorkerThread->scan(mScanner.data(), true);
}

void MainWindow::scan()
{
    disableScannerBindings();
    mImageItem->clear();
    mImageItem->setPos(mScanner->getBounds().topLeft());
    mScanningItem = mImageItem;
    mWorkerThread->scan(mScanner.data(), false);
}

void MainWindow::handleScanStarted(QImage image)
{
    mScanningItem->setImage(image);
}

void MainWindow::handleScanComplete(int status)
{
    mScanningItem = nullptr;
    enableScannerBindings();
    enableSaveButton();
}

void MainWindow::handleScanLineScanned(QByteArray scanLine)
{
    mScanningItem->setNextScanLine(scanLine);
}

void MainWindow::browse()
{
    const auto path = QFileDialog::getExistingDirectory(
        this, {}, ui->comboFolder->currentData().toString());
    if (!path.isEmpty())
        addFolder(path);
}

void MainWindow::addFolder(const QString &path)
{
    const auto dir = QDir(path);
    if (path.isEmpty() || !dir.exists())
        return;

    ui->comboFolder->insertItem(0, dir.dirName(), dir.path());
    ui->comboFolder->setCurrentIndex(0);
    while (ui->comboFolder->count() > 10)
        ui->comboFolder->removeItem(10);
}

void MainWindow::enableSaveButton()
{
    ui->buttonSave->setEnabled(
        !mImageItem->image().isNull() &&
        !ui->comboFolder->currentText().isEmpty() &&
        !ui->title->text().isEmpty());
}

void MainWindow::save()
{
    const auto dir = QDir(ui->comboFolder->currentData().toString());
    const auto index = ui->spinBoxIndex->value();

    auto filename = ui->title->text();
    if (ui->checkBoxIndexed->isChecked()) {
        filename += ui->indexSeparator->text();
        filename += QString::number(index);
    }
    filename += ".jpg";

    if (QFileInfo(dir.filePath(filename)).exists())
        if (QMessageBox(QMessageBox::Warning, QCoreApplication::applicationName(),
            tr("A file named \"{}\" already exists. Do you want to replace it?").arg(filename),
            QMessageBox::Cancel | QMessageBox::Yes).exec() != QMessageBox::Yes)
            return;

    if (!mImageItem->image().save(dir.filePath(filename), nullptr, 90)) {
        QMessageBox(QMessageBox::Warning, QCoreApplication::applicationName(),
            tr("Writing image file failed")).exec();
        return;
    }

    ui->buttonSave->setEnabled(false);
    if (ui->checkBoxIndexed->isChecked())
        ui->spinBoxIndex->setValue(index + 1);
}

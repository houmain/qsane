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

    ui->widgetIndex->setEnabled(false);
    ui->groupBoxProperties->setVisible(false);

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
        this, &MainWindow::updateSaveButton);
    connect(ui->title, &QLineEdit::textChanged,
        this, &MainWindow::updateSaveButton);
    connect(ui->pageView, &PageView::mousePressed,
        this, &MainWindow::handlePageViewMousePressed);
    connect(ui->pageView, &PageView::zoomChanged,
        [this](qreal scale) { mCropRect->setHandleSize(4.0 / scale); });
    connect(ui->buttonPreview, &QPushButton::clicked, this, &MainWindow::preview);
    connect(ui->buttonScan, &QPushButton::clicked, this, &MainWindow::scan);

    connect(ui->comboDevice, &QComboBox::currentIndexChanged,
        this, &MainWindow::handleDeviceIndexChanged);
    connect(mWorkerThread, &WorkerThread::scanStarted,
        this, &MainWindow::handleScanStarted);
    connect(mWorkerThread, &WorkerThread::scanComplete,
        this, &MainWindow::handleScanComplete);
    connect(mWorkerThread, &WorkerThread::scanLineScanned,
        this, &MainWindow::handleScanLineScanned);

    readSettings();
    updateScanButtons();
    updateSaveButton();
    QTimer::singleShot(500, this, &MainWindow::refreshDevices);
}

MainWindow::~MainWindow()
{
    delete mWorkerThread;
    delete ui;
    closeScanner();
    Scanner::shutdown();
}

void MainWindow::readSettings()
{
    auto &s = *mSettings;
    s.beginGroup("General");

    if (!restoreGeometry(s.value("geometry").toByteArray()))
        setGeometry(100, 100, 800, 600);
    else if (s.value("maximized").toBool())
        showMaximized();

    mSource = s.value("source").toString();
    mResolution = s.value("resolution").toDouble();
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

    s.setValue("source", mSource);
    s.setValue("resolution", mResolution);
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
    const auto devices = Scanner::initialize();
    ui->comboDevice->clear();
    for (const auto &device : devices)
        ui->comboDevice->addItem(device.vendor + " " + device.model, device.name);
    updateScanButtons();
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

    mScanner.reset(new Scanner(deviceName));
    if (mScanner->isOpened()) {
        connect(mScanner.data(), &Scanner::optionValuesChanged,
            this, &MainWindow::refreshControls);

        refreshControls();

        if (mScanner->getSource() != mSource)
            mScanner->setSource(mSource);

        const auto resolution = mScanner->getResolution();
        if (resolution.x() != mResolution || resolution.y() != mResolution)
            mScanner->setResolution({ mResolution, mResolution });
    }
    else {
        QMessageBox(QMessageBox::Warning, QCoreApplication::applicationName(),
            tr("Opening scanner failed"));
        closeScanner();
    }
}

void MainWindow::closeScanner()
{
    if (mScanner) {
        disconnect(mScanner.data(), &Scanner::optionValuesChanged,
            this, &MainWindow::refreshControls);
        mScanner.reset();
    }
}

void MainWindow::refreshControls()
{
    disconnect(ui->comboSource, &QComboBox::currentIndexChanged,
        this, &MainWindow::handleSourceChanged);
    disconnect(ui->comboResolution, &QComboBox::currentIndexChanged,
        this, &MainWindow::handleResolutionChanged);
    disconnect(mCropRect, &CropRect::transforming,
        this, &MainWindow::handleCropRectTransforming);

    ui->comboSource->clear();
    for (const auto &source : mScanner->getSources())
        ui->comboSource->addItem(tr(qPrintable(source)), source);
    ui->comboSource->setCurrentIndex(
        ui->comboSource->findData(mScanner->getSource()));

    const auto resolutions = mScanner->getUniformResolutions();
    ui->comboResolution->clear();
    for (auto resolution : resolutions)
        ui->comboResolution->addItem(QString::number(resolution), resolution);
    ui->comboResolution->setCurrentIndex(
        ui->comboResolution->findData(mResolution));

    const auto maximumBounds = mScanner->getMaximumBounds();
    ui->pageView->setBounds(maximumBounds);
    mCropRect->setMaximumBounds(maximumBounds);

    connect(ui->comboSource, &QComboBox::currentIndexChanged,
        this, &MainWindow::handleSourceChanged);
    connect(ui->comboResolution, &QComboBox::currentIndexChanged,
        this, &MainWindow::handleResolutionChanged);
    connect(mCropRect, &CropRect::transforming,
        this, &MainWindow::handleCropRectTransforming);
}

void MainWindow::handleSourceChanged(int index)
{
    const auto source = ui->comboSource->itemData(index).toString();
    if (!source.isEmpty()) {
        mSource = source;
        mScanner->setSource(source);

        mImageItem->clear();
        mCropRect->setBounds({});
    }
}

void MainWindow::handleResolutionChanged(int index)
{
    const auto resolution = ui->comboResolution->itemData(index).toDouble();
    if (resolution) {
        mResolution = resolution;
        mScanner->setResolution({ resolution, resolution });
    }
}

void MainWindow::handlePageViewMousePressed(const QPointF &position)
{
    mScene->addItem(mCropRect);
    mCropRect->startRect(position);
    updateScanButtons();
}

void MainWindow::handleCropRectTransforming(const QRectF &bounds)
{
    mScanner->setBounds(bounds);
    updateScanButtons();
}

void MainWindow::preview()
{
    if (mScanningItem)
        return;
    mImageItem->clear();
    mScanningItem = mPreviewItem;
    mWorkerThread->scan(mScanner.data(), true);
    updateScanButtons();
}

void MainWindow::scan()
{
    if (mScanningItem)
        return;
    mImageItem->clear();
    mImageItem->setPos(mScanner->getBounds().topLeft());
    mScanningItem = mImageItem;
    mWorkerThread->scan(mScanner.data(), false);
    updateScanButtons();
}

void MainWindow::handleScanStarted(QImage image)
{
    mScanningItem->setImage(image);
}

void MainWindow::handleScanLineScanned(QByteArray scanLine)
{
    mScanningItem->setNextScanLine(scanLine);
}

void MainWindow::handleScanComplete(bool succeeded)
{
    mScanningItem = nullptr;
    updateScanButtons();
    updateSaveButton();
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

    if (auto index = ui->comboFolder->findData(dir.path()); index >= 0)
        ui->comboFolder->removeItem(index);

    ui->comboFolder->insertItem(0, dir.dirName(), dir.path());
    ui->comboFolder->setCurrentIndex(0);
    while (ui->comboFolder->count() > 10)
        ui->comboFolder->removeItem(10);
}

void MainWindow::updateScanButtons()
{
    const auto canScan = (mScanner && !mScanningItem);
    ui->buttonPreview->setEnabled(canScan);
    ui->buttonScan->setEnabled(canScan && !mCropRect->bounds().isEmpty());
}

void MainWindow::updateSaveButton()
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

    if (QFileInfo::exists(dir.filePath(filename)))
        if (QMessageBox(QMessageBox::Warning, QCoreApplication::applicationName(),
            tr("A file named \"%1\" already exists.\nDo you want to replace it?").arg(filename),
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

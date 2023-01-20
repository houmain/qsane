#pragma once

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class Scanner;
class QSettings;
class WorkerThread;
class QGraphicsScene;
class GraphicsImageItem;
class CropRect;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public Q_SLOTS:
    void refreshDevices();
    void preview();
    void scan();
    void browse();
    void save();
    void togglePropertyBrowser();

private Q_SLOTS:
    void refreshControls();
    void handleDeviceIndexChanged(int index);
    void updateScanButtons();
    void updateSaveButton();
    void handleScanStarted(QImage image);
    void handleScanLineScanned(QByteArray scanLine);
    void handleScanComplete(bool succeeded);
    void handleResolutionChanged(int index);
    void handleCropRectTransforming(const QRectF &);
    void handlePageViewMousePressed(const QPointF &position);

protected:
    void keyPressEvent(QKeyEvent *event);
    void closeEvent(QCloseEvent *event);

private:
    void openScanner(const QString &deviceName);
    void closeScanner();
    void addFolder(const QString &path);
    void readSettings();
    void writeSettings();

    Ui::MainWindow *ui;
    QSettings *mSettings;
    WorkerThread *mWorkerThread;
    QScopedPointer<Scanner> mScanner;

    QGraphicsScene *mScene{ };
    CropRect *mCropRect{ };
    GraphicsImageItem *mPreviewItem{ };
    GraphicsImageItem *mImageItem{ };
    GraphicsImageItem *mScanningItem{ };
    double mResolution{ };
};

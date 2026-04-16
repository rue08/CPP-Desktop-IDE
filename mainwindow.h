#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSplitter>
#include <QSettings>
#include <QLabel>
#include <QTreeWidget>
#include <QVariant>
#include <QPlainTextEdit>
#include <QFileDialog>
#include <QStackedWidget>
#include "storage.h"
#include "loginwindow.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    QTabWidget *theWorkspace;
    QPlainTextEdit* curr;

private slots:
    void closeTab(int index);

    void on_actionNew_File_triggered();

    void on_actionThe_Vault_triggered();

    void on_actionOpen_triggered();

    void on_actionOpen_Folder_triggered();

    void on_actionUndo_triggered();

    void on_actionRedo_triggered();

    void on_actionCut_triggered();

    void on_actionCopy_triggered();

    void on_actionPaste_triggered();

    void on_actionSave_triggered();

    void on_actionClose_File_triggered();

    void localFilesItemClicked(QTreeWidgetItem* item);

    void cloudFilesItemClicked(QTreeWidgetItem* item);

    void on_actionRun_triggered();

    void on_actionLogin_triggered();

    void on_actionUpload_triggered();

    void onSetCloudFiles(const QString &fileName, const QString &cloudFilePath);

    void onDownloadFile(const QByteArray &response);

    void onEnableActionUpload(bool flag, const QString& idToken, const QString& uid);

    void on_actionClose_Folder_triggered();

private:
    Ui::MainWindow *ui;
    QStackedWidget *localFilesStack;
    QStackedWidget *cloudFilesStack;
    QTreeWidget *localFiles;
    QTreeWidget *cloudFiles;
    QSplitter *splitter;
    QSplitter* theVault;
    QSettings settings;
    QString filePath;
    QString projectFolderPath;
    QTabBar tabBar;
    QFileInfo info;
    Storage *storage;
    LoginWindow *loginWindow;
    QLabel* localFilesArea;
    QLabel* cloudFilesArea;

protected:
    void closeEvent(QCloseEvent *event) override;
};
#endif // MAINWINDOW_H

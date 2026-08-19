#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSplitter>
#include <QSettings>
#include <QLabel>
#include <QTreeWidget>
#include <QIcon>
#include <QVariant>
#include <QFileDialog>
#include <QStackedWidget>
#include <QSet>
#include "storage.h"
#include "loginwindow.h"
#include "monacoeditor.h"
#include "theme.h"

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
    MonacoEditor* curr;

private slots:
    void closeTab(int index);

    void on_actionNew_File_triggered();

    void on_actionNew_Text_File_triggered();

    void on_actionNew_Markdown_File_triggered();

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

    void onDownloadFile(const QByteArray &response, QObject *targetTab);

    void onEnableActionUpload(bool flag, const QString& idToken, const QString& uid);

    void on_actionClose_Folder_triggered();

    void onUploadSucceeded(const QString &localFilePath, const QString &cloudFilePath);
    void onUploadFailed(const QString &localFilePath, const QString &errorString);

    // Reports how many uploads succeeded once a wave (a single upload, or a
    // batch's worth queued back-to-back) has fully settled -- see
    // Storage::uploadBatchFinished().
    void onUploadBatchFinished(int succeededCount);
    void onListFilesFailed(const QString &errorString);
    void onDownloadFailed(const QString &errorString, QObject *targetTab);

    void onTokenRefreshRequired();
    void onIdTokenRefreshed(const QString &idToken);
    void onSessionExpired();

    void onBackendLoginSucceeded();
    void onBackendLoginFailed(const QString &errorString);
    void on_actionSettings_triggered();

    void on_actionToggle_Comment_triggered();

    // Fired by QStyleHints whenever the effective color scheme changes --
    // either the OS's own appearance flipping while the theme menu is set
    // to "System", or setThemeMode() itself applying an explicit Light/Dark
    // override (see theme.h). Either way, this is the single place that
    // re-derives the effective scheme and pushes it out to the native UI
    // and every open Monaco tab.
    void onColorSchemeChanged();

private:
    // Marks `tab`'s title with a "● " prefix while its document has unsaved
    // changes, clearing it again once saved -- looked up by pointer rather
    // than a captured index since tabs are reorderable (setMovable(true)).
    void wireModifiedIndicator(MonacoEditor *tab);

    // Shared by the New File / New Text File / New Markdown File actions --
    // opens a blank tab titled "<titlePrefix> <n>[.defaultExtension]" and
    // tags it with the extension Save-As should fall back to if the user
    // saves without typing one of its own (see on_actionSave_triggered()).
    void newFileTab(const QString &titlePrefix, const QString &defaultExtension);

    // Picks the file-tree icon for a given file name by its extension --
    // shared between the local "Open Folder" tree and the cloud files tree
    // so both classify extensions the same way. Falls back to a null QIcon
    // (no icon shown) for anything outside the IDE's recognized C++/docs
    // extensions. Reads currentThemeIsDark for the one icon (the plain-text
    // fallback) whose tint actually differs between themes -- see
    // applyTheme().
    QIcon iconForFileName(const QString &fileName);

    // True if the cloud files tree already has a top-level entry named
    // `fileName` -- used before uploading a *local* file for the first time
    // to warn if it would silently overwrite an unrelated cloud file that
    // just happens to share the same basename (cloud files are keyed on
    // filename alone server-side, not full path). Not used for re-uploading
    // a tab already known to be that exact cloud file (isCloudFile == true),
    // since overwriting there is the intended action.
    bool cloudFileExists(const QString &fileName) const;

    // The output_circle "this is an executable" icon shown in the local
    // files tree -- pulled out of on_actionOpen_Folder_triggered() so
    // refreshTreeIcons() can reuse the same theme-aware choice.
    QIcon iconForExecutable();

    // Loads an SVG file-tree icon such that it looks identical whether its
    // row is selected or not. Without this, Qt's item delegate asks QIcon
    // for a QIcon::Selected-mode pixmap on a selected row, and since a
    // plain QIcon(path) never registers one, Qt auto-generates a
    // desaturated/washed-out substitute -- explicitly registering the same
    // source for both modes here means there's nothing left for it to
    // generate. Used by iconForFileName()/iconForExecutable() and the
    // upload-success checkmark in onSetCloudFiles(), i.e. every icon shown
    // inside localFiles/cloudFiles.
    static QIcon loadFileTreeIcon(const QString &path);

    // Applies `mode` via Theme::setMode() and immediately re-derives and
    // pushes the effective scheme, rather than only relying on
    // onColorSchemeChanged() -- QStyleHints::colorSchemeChanged only fires
    // on an actual change, so picking "Light" while the OS (and thus the
    // prior System-mode effective scheme) is already light would otherwise
    // leave the UI unrefreshed on first selection.
    void setThemeMode(Theme::Mode mode);

    // Pushes `isDark` out to everything that isn't palette-driven: the
    // toolbar/menu icons (flat black/white glyphs, not tinted by QPalette),
    // the file-tree hover overlay and placeholder-label colors, the
    // already-populated file trees, and every open Monaco tab. QStyleHints'
    // color scheme itself (set by setThemeMode()/Theme::setMode()) already
    // handles ordinary native widget chrome -- this covers what it can't.
    void applyTheme(bool isDark);

    // Re-icons whatever's already in the local files tree after a theme
    // change -- see applyTheme(). Deliberately leaves the cloud files tree
    // alone: one of its icons can be a transient upload-success checkmark
    // (onSetCloudFiles()) that isn't recoverable from the item alone, and
    // the only icon that's actually theme-sensitive (the plain-text
    // fallback) is a subtle gray-on-gray difference either way -- it
    // corrects itself on the tree's next refresh regardless.
    void refreshTreeIcons();

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

    // Mirrors Theme::isDark() -- kept as a member (rather than re-querying
    // Theme::isDark() every time) so iconForFileName()/iconForExecutable()
    // can read it without needing every call site updated. Set by
    // applyTheme(), which is what actually changes it.
    bool currentThemeIsDark = true;

    // Cloud paths whose upload just succeeded and are waiting for the next
    // onSetCloudFiles() pass to be shown with a success icon.
    QSet<QString> recentlyUploadedCloudPaths;

    // Guards against onSessionExpired() re-entering loginWindow->exec() --
    // a cascade of requests failing against the same dead session (e.g. a
    // retried listFiles() after abandonPendingRetries()) can each emit
    // sessionExpired() again while the first dialog is still showing.
    bool handlingSessionExpiry = false;

protected:
    void closeEvent(QCloseEvent *event) override;

    // Keeps the file trees' pointing-hand cursor scoped to actual rows.
    // A plain setCursor() on the tree widget itself covers its whole
    // viewport, hand included over the empty space below the last item --
    // this watches localFiles'/cloudFiles' viewports and swaps the cursor
    // based on whether indexAt() under the mouse is actually valid.
    bool eventFilter(QObject *watched, QEvent *event) override;
};
#endif // MAINWINDOW_H

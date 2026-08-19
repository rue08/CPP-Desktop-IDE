#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "monacoeditor.h"
#include "terminal.h"
#include <QCloseEvent>
#include <QMouseEvent>
#include <QMessageBox>
#include <QPushButton>
#include <QDir>
#include <QDirIterator>
#include <QStackedWidget>
#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QGuiApplication>
#include <QStyleHints>
#include <QActionGroup>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Reapplies whatever theme mode was last chosen from the View > Theme
    // menu -- QStyleHints's color-scheme override doesn't itself persist
    // across process restarts, only the QSettings value backing
    // Theme::mode() does. Also reacts live to further changes, whether from
    // the OS's own appearance (while in "System" mode) or setThemeMode()
    // applying an explicit override.
    Theme::setMode(Theme::mode());
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, &MainWindow::onColorSchemeChanged);

    projectFolderPath = QStringLiteral(PROJECT_ROOT_DIR);

    storage = new Storage(this);
    loginWindow = new LoginWindow(this, this);

    // No hardcoded default -- backend URL changes on every ngrok tunnel
    // restart, so it's read fresh from settings each launch and only ever
    // updated through the Settings action, never compiled in.
    storage -> setBackendUrl(settings.value("backendUrl").toString());

    splitter = new QSplitter(Qt::Horizontal);
    setCentralWidget(splitter);

    theVault = new QSplitter(Qt::Vertical, splitter);

    localFilesStack = new QStackedWidget(theVault);
    cloudFilesStack = new QStackedWidget(theVault);

    localFiles = new QTreeWidget;
    localFilesStack->addWidget(localFiles);

    cloudFiles = new QTreeWidget;
    cloudFilesStack->addWidget(cloudFiles);

    theWorkspace = new QTabWidget(splitter);

    theVault -> setMinimumWidth(105);

    QWidget *spacer = new QWidget();
    spacer -> setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    ui -> toolBar -> addWidget(spacer);
    ui -> toolBar -> addAction(ui -> actionRun);
    ui -> toolBar -> addAction(ui -> actionLogin);
    ui -> toolBar -> addAction(ui -> actionLogout);
    ui -> toolBar -> addAction(ui -> actionSettings);
    ui -> actionUpload -> setEnabled(false);
    ui -> actionLogout -> setEnabled(false);
    ui -> actionUpload_File_to_Cloud -> setEnabled(false);
    ui -> actionDelete_File_from_Cloud -> setEnabled(false);


    // View > Theme -- System/Light/Dark, mutually exclusive. Built
    // programmatically rather than in mainwindow.ui, same as the toolbar
    // actions above, since its checked state has to be derived from
    // Theme::mode() at startup rather than a fixed .ui default.
    QMenu *viewMenu = ui -> menubar -> addMenu("View");
    QMenu *themeMenu = viewMenu -> addMenu("Theme");

    QAction *systemThemeAction = themeMenu -> addAction("System");
    QAction *lightThemeAction = themeMenu -> addAction("Light");
    QAction *darkThemeAction = themeMenu -> addAction("Dark");

    QActionGroup *themeGroup = new QActionGroup(this);
    for (QAction *action : {systemThemeAction, lightThemeAction, darkThemeAction})
    {
        action -> setCheckable(true);
        themeGroup -> addAction(action);
    }

    switch (Theme::mode())
    {
    case Theme::Mode::Light: lightThemeAction -> setChecked(true); break;
    case Theme::Mode::Dark: darkThemeAction -> setChecked(true); break;
    case Theme::Mode::System: default: systemThemeAction -> setChecked(true); break;
    }

    connect(systemThemeAction, &QAction::triggered, this, [this]() { setThemeMode(Theme::Mode::System); });
    connect(lightThemeAction, &QAction::triggered, this, [this]() { setThemeMode(Theme::Mode::Light); });
    connect(darkThemeAction, &QAction::triggered, this, [this]() { setThemeMode(Theme::Mode::Dark); });

    theWorkspace -> setMovable(true);
    theWorkspace -> setTabsClosable(true);
    theWorkspace -> setTabShape(QTabWidget::Triangular);

    // Connectors
    connect(theWorkspace, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);

    localFiles -> setHeaderHidden(true);
    localFiles -> setColumnCount(1);
    localFiles -> setSelectionMode(QAbstractItemView::ExtendedSelection);

    cloudFiles -> setHeaderHidden(true);
    cloudFiles -> setColumnCount(1);
    cloudFiles -> setSelectionMode(QAbstractItemView::ExtendedSelection);

    // Files are double-click-to-open but previously gave zero indication of
    // that -- no cursor change, no hover feedback, same as any inert label.
    // Hand cursor matches the treatment already used on the Login button
    // (loginwindow.ui) for other clickable things; the hover highlight's
    // actual color (originally a hardcoded low-alpha white) is filled in by
    // applyTheme() below, since a white overlay meant to stay subtle against
    // a dark background does the opposite against a light one.
    //
    // Cursor is scoped to actual rows via eventFilter() rather than a plain
    // setCursor() here -- that would cover the tree's whole viewport, hand
    // included over the empty space below the last item, which is
    // misleading since there's nothing to click there.
    localFiles -> viewport() -> setMouseTracking(true);
    cloudFiles -> viewport() -> setMouseTracking(true);
    localFiles -> viewport() -> installEventFilter(this);
    cloudFiles -> viewport() -> installEventFilter(this);


    localFilesArea = new QLabel("Local Files' Area");
    localFilesStack -> addWidget(localFilesArea);
    localFilesArea->setAlignment(Qt::AlignCenter);
    localFilesArea->setWordWrap(true);
    localFilesArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    cloudFilesArea = new QLabel("Cloud Files' Area");
    cloudFilesStack -> addWidget(cloudFilesArea);
    cloudFilesArea->setAlignment(Qt::AlignCenter);
    cloudFilesArea->setWordWrap(true);
    cloudFilesArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    // Both labels' muted text color is theme-dependent -- set by
    // applyTheme() below, same reasoning as the hover style above.

    localFilesStack -> setCurrentWidget(localFilesArea);
    cloudFilesStack -> setCurrentWidget(cloudFilesArea);

    connect(localFiles, &QTreeWidget::itemDoubleClicked, this, &MainWindow::localFilesItemClicked);
    connect(cloudFiles, &QTreeWidget::itemDoubleClicked, this, &MainWindow::cloudFilesItemClicked);
    connect(storage, &Storage::cloudFilesCleared, cloudFiles, &QTreeWidget::clear);
    connect(storage, &Storage::setCloudFiles, this, &MainWindow::onSetCloudFiles);
    connect(storage, &Storage::setDownloadFile, this, &MainWindow::onDownloadFile);
    connect(storage, &Storage::uploadSucceeded, this, &MainWindow::onUploadSucceeded);
    connect(storage, &Storage::uploadFailed, this, &MainWindow::onUploadFailed);
    connect(storage, &Storage::uploadBatchFinished, this, &MainWindow::onUploadBatchFinished);
    connect(storage, &Storage::deleteSucceeded, this, &MainWindow::onDeleteSucceeded);
    connect(storage, &Storage::deleteFailed, this, &MainWindow::onDeleteFailed);
    connect(storage, &Storage::deleteBatchFinished, this, &MainWindow::onDeleteBatchFinished);
    connect(storage, &Storage::listFilesFailed, this, &MainWindow::onListFilesFailed);
    connect(storage, &Storage::downloadFailed, this, &MainWindow::onDownloadFailed);
    connect(storage, &Storage::tokenRefreshRequired, this, &MainWindow::onTokenRefreshRequired);
    connect(storage, &Storage::backendLoginSucceeded, this, &MainWindow::onBackendLoginSucceeded);
    connect(storage, &Storage::backendLoginFailed, this, &MainWindow::onBackendLoginFailed);
    connect(loginWindow, &LoginWindow::enableActionUpload, this, &MainWindow::onEnableActionUpload);
    connect(loginWindow, &LoginWindow::idTokenRefreshed, this, &MainWindow::onIdTokenRefreshed);
    connect(loginWindow, &LoginWindow::sessionExpired, this, &MainWindow::onSessionExpired);

    // Silently signs back in from a previous run, if a session was saved --
    // a no-op otherwise (nothing saved). Deliberately after every connect()
    // above: the actual response only ever arrives asynchronously once the
    // event loop is running (well after this constructor returns), but
    // keeping it below everything it depends on avoids any doubt about that.
    loginWindow -> restoreSession();

    if (!settings.contains("splitterDimensions"))
        splitter -> setSizes({100, 100});

    // Not letting the workspace completely collapse
    splitter -> setCollapsible(1, false);
    // Setting minimum width for the workspace
    theWorkspace -> setMinimumWidth(100);

    // Remembering the previous sizes of each layout
    splitter -> restoreState(settings.value("splitterDimensions").toByteArray());

    // Seeds icons/stylesheets for the initial paint -- deliberately last in
    // the constructor, since applyTheme() touches localFilesArea/
    // cloudFilesArea (among other things constructed above), and calling it
    // any earlier than everything it touches exists is a use of
    // uninitialized member pointers. Can't rely solely on
    // onColorSchemeChanged() for this either, since Theme::setMode() near
    // the top only emits colorSchemeChanged when the effective scheme
    // actually changes.
    applyTheme(Theme::isDark());
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_actionThe_Vault_triggered()
{
    if (theVault -> isHidden())
        theVault -> show();
    else
        theVault -> hide();
}


void MainWindow::closeTab(int index)
{
    curr = qobject_cast<MonacoEditor*>(theWorkspace -> widget(index));

    QMessageBox msgBox(this);
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Save);
    msgBox.setEscapeButton(QMessageBox::Cancel);

    filePath = curr -> property("filePath").toString();

    if (curr -> property("isCloudFile").toBool())
    {
        // Nothing to reconcile if the tab's content matches what was last
        // downloaded/uploaded -- mirrors the unchanged-file fast path just
        // below for local files.
        if (!curr -> isModified())
        {
            theWorkspace -> removeTab(index);
            delete curr;
            return;
        }

        QMessageBox msgBox(this);
        msgBox.setIcon(QMessageBox::Information);

        QPushButton *uploadButton = msgBox.addButton("Upload", QMessageBox::AcceptRole);
        msgBox.setDefaultButton(uploadButton);

        QPushButton *saveButton = msgBox.addButton("Save", QMessageBox::ActionRole);

        QPushButton *closeButton = msgBox.addButton("Close Tab", QMessageBox::ActionRole);

        msgBox.addButton(QMessageBox::Cancel);
        msgBox.setEscapeButton(QMessageBox::Cancel);

        msgBox.setText("Choose whether to upload or save the document.");
        msgBox.exec();

        if (msgBox.clickedButton() -> text() == uploadButton->text())
            storage -> uploadFile(curr -> toPlainText().toUtf8(), curr -> property("cloudFileName").toString());
        else if (msgBox.clickedButton() -> text() == saveButton->text())
            on_actionSave_triggered();
        else if (msgBox.clickedButton()->text() == closeButton->text())
            theWorkspace -> removeTab(index);

        return;
    }

    if (filePath == "")
    {
        if ((curr -> toPlainText()).isEmpty())
        {
            theWorkspace -> removeTab(index);
            delete curr;
            return;
        }
        else
            msgBox.setText("The document has not been saved.");
    }
    else
    {
        QFile closeFile(filePath);

        if (!closeFile.open(QIODevice::ReadOnly | QIODevice::Text) || curr -> toPlainText() != closeFile.readAll())
            msgBox.setText("The document has been modified.");
        else
        {
            theWorkspace -> removeTab(index);
            delete curr;
            return;
        }
    }

    msgBox.setInformativeText("Do you want to save your changes?");

    int reply = msgBox.exec();

    if (reply == QMessageBox::Discard)
    {
        theWorkspace->removeTab(index);
        delete curr;
        return;
    }
    else if (reply == QMessageBox::Cancel)
        return;
    else if (reply == QMessageBox::Save)
    {
        on_actionSave_triggered();
        if (!filePath.isEmpty())
        {
            theWorkspace->removeTab(index);
            delete curr;
        }
    }
}


QIcon MainWindow::iconForFileName(const QString &fileName)
{
    // Extensions this C++ IDE actually recognizes -- C++ source/header
    // variants, plus the docs commonly kept alongside C++ code. Anything
    // else falls through to a null QIcon (no icon shown), same as before.
    //
    // Icons are pulled from VS Code's own default "Seti" icon theme
    // (microsoft/vscode, extensions/theme-seti -- MIT licensed, ultimately
    // sourced from jesseweed/seti-ui) so files look the way they would in
    // VS Code itself. Headers split into two icons, matching Seti's own
    // distinction: plain .h gets the "C" glyph ("_c_1"), while
    // .hpp/.hh/.hxx/.h++ get the "C++" glyph ("_cpp_1") -- both purple,
    // vs. blue for actual source files. .txt falls back to Seti's generic
    // gray "_default" file glyph.
    static const QSet<QString> cppSourceExtensions = {"cpp", "cc", "cxx", "c++"};
    static const QSet<QString> cppPlusPlusHeaderExtensions = {"hpp", "hh", "hxx", "h++"};

    QString suffix = QFileInfo(fileName).suffix().toLower();

    if (cppSourceExtensions.contains(suffix))
        return loadFileTreeIcon(":/icons/Icons/seti_cpp_24dp_519ABA.svg");
    if (suffix == "h")
        return loadFileTreeIcon(":/icons/Icons/seti_h_24dp_A074C4.svg");
    if (cppPlusPlusHeaderExtensions.contains(suffix))
        return loadFileTreeIcon(":/icons/Icons/seti_hpp_24dp_A074C4.svg");
    if (suffix == "md")
        return loadFileTreeIcon(":/icons/Icons/seti_markdown_24dp_519ABA.svg");
    if (suffix == "txt")
        // The one Seti icon actually swapped per-theme (see applyTheme()) --
        // everything else above keeps its VS Code Seti color regardless of
        // theme, same as VS Code itself does.
        return loadFileTreeIcon(currentThemeIsDark
            ? ":/icons/Icons/seti_default_24dp_D4D7D6.svg"
            : ":/icons/Icons/seti_default_24dp_6E7681.svg");

    return QIcon();
}


QIcon MainWindow::iconForExecutable()
{
    return loadFileTreeIcon(currentThemeIsDark
        ? ":/icons/Icons/output_circle_24dp_FFFFFF_FILL0_wght400_GRAD0_opsz24.svg"
        : ":/icons/Icons/output_circle_24dp_000000_FILL0_wght400_GRAD0_opsz24.svg");
}


QIcon MainWindow::loadFileTreeIcon(const QString &path)
{
    QIcon icon;
    icon.addFile(path, QSize(), QIcon::Normal);
    icon.addFile(path, QSize(), QIcon::Selected);
    return icon;
}


void MainWindow::wireModifiedIndicator(MonacoEditor *tab)
{
    connect(tab, &MonacoEditor::modifiedChanged, this, [this, tab](bool modified) {
        int index = theWorkspace -> indexOf(tab);
        if (index == -1)
            return; // tab's been closed since; nothing left to update

        QString title = theWorkspace -> tabText(index);
        if (title.startsWith("● "))
            title.remove(0, 2);
        if (modified)
            title.prepend("● ");

        theWorkspace -> setTabText(index, title);
    });
}


void MainWindow::newFileTab(const QString &titlePrefix, const QString &defaultExtension)
{
    QString title = defaultExtension.isEmpty()
        ? QString("%1 %2").arg(titlePrefix).arg(theWorkspace -> count() + 1)
        : QString("%1 %2.%3").arg(titlePrefix).arg(theWorkspace -> count() + 1).arg(defaultExtension);

    theWorkspace -> addTab(new MonacoEditor(), title);
    curr = qobject_cast<MonacoEditor*>(theWorkspace -> currentWidget());
    curr -> setProperty("filePath", QVariant(""));

    // Read back by on_actionSave_triggered()'s Save-As fallback so a blank
    // tab saved with no extension typed lands on the extension it was
    // actually created for, instead of always defaulting to .cpp.
    curr -> setProperty("defaultExtension", QVariant(defaultExtension.isEmpty() ? QStringLiteral("cpp") : defaultExtension));

    theWorkspace -> setCurrentIndex(theWorkspace -> count() - 1);

    wireModifiedIndicator(curr);
}


void MainWindow::on_actionNew_File_triggered()
{
    newFileTab("Tab", "");
}


void MainWindow::on_actionNew_Text_File_triggered()
{
    newFileTab("Untitled", "txt");
}


void MainWindow::on_actionNew_Markdown_File_triggered()
{
    newFileTab("Untitled", "md");
}


void MainWindow::on_actionOpen_triggered()
{
    // Scoped to the extensions this IDE actually recognizes (see
    // iconForFileName()) -- deliberately no "All Files" catch-all, this is
    // a C++ IDE, not a general-purpose file browser.
    filePath = QFileDialog::getOpenFileName(this, "", projectFolderPath,
        "C++ Source Files (*.cpp *.cc *.cxx *.c++);;"
        "Header Files (*.h *.hpp *.hh *.hxx *.h++);;"
        "Docs (*.md *.txt)");
    QFile openFile(filePath);

    if (filePath.isEmpty())
        return;

    if (!openFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        statusBar() -> showMessage("Couldn't open \"" + QFileInfo(filePath).fileName() + "\".", 4000);
        return;
    }

    info = QFileInfo(filePath);
    theWorkspace->addTab(new MonacoEditor(), info.fileName());
    theWorkspace -> setCurrentIndex(theWorkspace -> count() - 1);

    curr = qobject_cast<MonacoEditor*>(theWorkspace -> currentWidget());
    curr -> setPlainText(openFile.readAll()); // resets the modified flag internally -- loading existing content isn't an edit

    curr -> setProperty("filePath", QVariant(filePath));

    wireModifiedIndicator(curr);

    openFile.close();
}


void MainWindow::on_actionOpen_Folder_triggered()
{
    QDir dir(projectFolderPath);
    QString folderPath = QFileDialog::getExistingDirectory(this, "", dir.path());

    if (folderPath.isEmpty())
        return;

    localFilesStack -> setCurrentWidget(localFiles);

    dir = QDir(folderPath);

    QTreeWidgetItem* root = new QTreeWidgetItem(localFiles);
    root -> setText(0, dir.dirName());
    root -> setIcon(0, QIcon::fromTheme("folder-open"));

    QDirIterator it(dir.path(), QDir::Files | QDir::NoDotAndDotDot);

    while (it.hasNext())
    {
        it.next();
        QTreeWidgetItem* child = new QTreeWidgetItem(root);
        child -> setText(0, it.fileName());

        info = QFileInfo(it.filePath());
        if (info.isExecutable())
            child -> setIcon(0, iconForExecutable());
        else
            child -> setIcon(0, iconForFileName(it.fileName()));

        child -> setData(0, Qt::UserRole, it.filePath());
    }
}


void MainWindow::on_actionSave_triggered()
{
    if (theWorkspace -> currentIndex() == -1)
        return;

    curr = qobject_cast<MonacoEditor*>(theWorkspace -> currentWidget());

    if (curr -> toPlainText().isEmpty())
    {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Information);
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setDefaultButton(QMessageBox::Ok);
        msgBox.setEscapeButton(QMessageBox::Ok);
        msgBox.setText("This file is empty.");
        msgBox.exec();

        return;
    }

    filePath =  curr -> property("filePath").toString();

    if (filePath == "")
    {
        filePath = QFileDialog::getSaveFileName(this, "", projectFolderPath,
            "C++ Source Files (*.cpp *.cc *.cxx *.c++);;"
            "Header Files (*.h *.hpp *.hh *.hxx *.h++);;"
            "Docs (*.md *.txt)");

        if (filePath.isEmpty())
            return;

        // Only fall back to an extension when nothing recognized was
        // typed/chosen at all -- respects an explicit .h/.md/.txt name
        // instead of overriding it. The fallback itself comes from
        // whichever "New ___ File" action created this tab (newFileTab()),
        // so a blank Markdown tab saved as bare "notes" lands on notes.md
        // rather than always defaulting to .cpp.
        if (iconForFileName(QFileInfo(filePath).fileName()).isNull())
        {
            QString defaultExtension = curr -> property("defaultExtension").toString();
            if (defaultExtension.isEmpty())
                defaultExtension = "cpp";
            filePath += "." + defaultExtension;
        }

        curr -> setProperty("filePath", QVariant(filePath));

        info = QFileInfo(filePath);
        theWorkspace -> setTabText(theWorkspace -> currentIndex(), info.fileName());

        // Deliberately falls through to the write below instead of
        // returning here -- this used to return right after picking a
        // filename, which meant the very first save of a new file renamed
        // the tab but never actually wrote it to disk.
    }


    QFile saveFile(filePath);

    if (!saveFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "Save Failed",
            "Couldn't write to \"" + QFileInfo(filePath).fileName() + "\":\n" + saveFile.errorString());
        return;
    }

    QTextStream out(&saveFile);
    out << curr -> toPlainText();

    saveFile.flush();
    saveFile.close();

    curr -> setModified(false);
}


void MainWindow::on_actionUndo_triggered()
{
    if (theWorkspace -> currentIndex() != -1)
        qobject_cast<MonacoEditor*>(theWorkspace -> currentWidget()) -> undo();
}


void MainWindow::on_actionRedo_triggered()
{
    if (theWorkspace -> currentIndex() != -1)
        qobject_cast<MonacoEditor*>(theWorkspace -> currentWidget()) -> redo();
}


void MainWindow::on_actionCut_triggered()
{
    if (theWorkspace -> currentIndex() != -1)
        qobject_cast<MonacoEditor*>(theWorkspace -> currentWidget()) -> cut();
}


void MainWindow::on_actionCopy_triggered()
{
    if (theWorkspace -> currentIndex() != -1)
        qobject_cast<MonacoEditor*>(theWorkspace -> currentWidget()) -> copy();
}


void MainWindow::on_actionPaste_triggered()
{
    if (theWorkspace -> currentIndex() != -1)
        qobject_cast<MonacoEditor*>(theWorkspace -> currentWidget()) -> paste();
}


void MainWindow::on_actionToggle_Comment_triggered()
{
    if (theWorkspace -> currentIndex() == -1)
        return;

    // Monaco's own comment-toggle command replaces the hand-rolled
    // line-comment logic this used to have -- it's language-aware and
    // already knows how to toggle a selection the same way VS Code does.
    // This action has no shortcut of its own anymore (see mainwindow.ui) --
    // Ctrl+/ is left to Monaco's own built-in binding for the same command
    // when the editor has focus, rather than having both fire for the same
    // keypress. This slot is now only reachable via the menu/toolbar entry.
    qobject_cast<MonacoEditor*>(theWorkspace -> currentWidget()) -> toggleCommentSelection();
}


void MainWindow::on_actionClose_File_triggered()
{
    if (theWorkspace->currentIndex() != -1)
        closeTab(theWorkspace->currentIndex());
    else
        this -> close();
}


void MainWindow::on_actionLogin_triggered()
{
    loginWindow -> setModal(true);
    loginWindow -> exec();
}


void MainWindow::on_actionLogout_triggered()
{
    loginWindow -> logOut();
    storage -> setIdToken(QString());

    ui -> actionUpload -> setEnabled(false);
    ui -> actionLogout -> setEnabled(false);
    ui -> actionUpload_File_to_Cloud -> setEnabled(false);
    ui -> actionDelete_File_from_Cloud -> setEnabled(false);

    cloudFiles -> clear();
    cloudFilesStack -> setCurrentWidget(cloudFilesArea);

    // Deliberately not reopening the login dialog (unlike onSessionExpired())
    // -- this was a deliberate action, not an error state that needs fixing
    // before the app is usable again.
    ui -> statusbar -> showMessage("Logged out.", 3000);
}

void MainWindow::onEnableActionUpload(bool flag, const QString& idToken, const QString& uid)
{
    Q_UNUSED(uid); // the backend derives identity from the token itself, server-side

    ui -> actionUpload -> setEnabled(flag);
    ui -> actionLogout -> setEnabled(flag);
    ui -> actionUpload_File_to_Cloud -> setEnabled(flag);
    ui -> actionDelete_File_from_Cloud -> setEnabled(flag);
    storage -> setIdToken(idToken);

    // Establishes/refreshes the users row for this session before anything
    // else is allowed to touch /files -- see onBackendLoginSucceeded().
    storage -> loginToBackend();
}


void MainWindow::onBackendLoginSucceeded()
{
    statusBar() -> showMessage("Successfully logged in.", 2000);
    storage -> listFiles();
}


void MainWindow::onBackendLoginFailed(const QString &errorString)
{
    ui -> actionUpload -> setEnabled(false);
    ui -> actionLogout -> setEnabled(false);
    ui -> actionUpload_File_to_Cloud -> setEnabled(false);
    ui -> actionDelete_File_from_Cloud -> setEnabled(false);
    statusBar() -> showMessage("Logged in successfully, but the cloud backend is unreachable.", 5000);
    QMessageBox::warning(this, "Backend Unavailable", errorString);
}


void MainWindow::on_actionSettings_triggered()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Settings");
    dialog.setMinimumWidth(440);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    layout -> addWidget(new QLabel("Backend URL (e.g. an ngrok tunnel):"));

    QLineEdit *urlEdit = new QLineEdit(settings.value("backendUrl").toString());
    layout -> addWidget(urlEdit);

    QPushButton *fetchButton = new QPushButton("Fetch Latest from GitHub");
    layout -> addWidget(fetchButton);

    QLabel *statusLabel = new QLabel();
    statusLabel -> setWordWrap(true);
    layout -> addWidget(statusLabel);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout -> addWidget(buttonBox);

    // Scoped to this dialog's lifetime via the &dialog context object --
    // Qt auto-disconnects these the moment dialog goes out of scope below,
    // so there's nothing left listening to storage's signals afterwards.
    connect(fetchButton, &QPushButton::clicked, &dialog, [this, &dialog, fetchButton, statusLabel]() {
        fetchButton -> setEnabled(false);
        statusLabel -> setText("Fetching...");
        dialog.adjustSize();
        storage -> fetchDiscoveryUrl();
    });
    connect(storage, &Storage::discoveryUrlFetched, &dialog, [&dialog, urlEdit, fetchButton, statusLabel](const QString &url) {
        urlEdit -> setText(url);
        statusLabel -> setText("Fetched the latest URL -- click OK to use it.");
        fetchButton -> setEnabled(true);
        // Changing statusLabel's wrapped text doesn't automatically grow the
        // dialog window on its own -- without this, the new text just
        // overlaps whatever's below it instead of pushing the window taller.
        dialog.adjustSize();
    });
    connect(storage, &Storage::discoveryUrlFetchFailed, &dialog, [&dialog, fetchButton, statusLabel](const QString &errorString) {
        statusLabel -> setText("Couldn't fetch the latest URL: " + errorString);
        fetchButton -> setEnabled(true);
        dialog.adjustSize();
    });

    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted)
        return;

    QString newUrl = urlEdit -> text().trimmed();
    while (newUrl.endsWith('/'))
        newUrl.chop(1);

    settings.setValue("backendUrl", newUrl);
    storage -> setBackendUrl(newUrl);
}


void MainWindow::on_actionRun_triggered()
{
    if (theWorkspace -> currentIndex() == -1)
    {
        statusBar() -> showMessage("Select a file to execute.", 3000);
        return;
    }
    on_actionSave_triggered();
    Terminal* myTerminal = new Terminal(this, this);
    myTerminal -> runFile();
}


void MainWindow::localFilesItemClicked(QTreeWidgetItem* item)
{
    filePath = item -> data(0, Qt::UserRole).toString();
    // Reuses iconForFileName()'s recognized-extension set as the single
    // source of truth for "is this a file type the IDE knows how to open" --
    // previously this only allowed anything containing ".cpp" (which also
    // loosely matched unrelated names like "notes.cpp.bak"), and silently
    // refused to open .h/.md/.txt files even though they're now shown with
    // their own icons in the tree.
    if (iconForFileName(QFileInfo(filePath).fileName()).isNull())
        return;
    info = QFileInfo(filePath);
    if (info.isExecutable())
    {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Information);
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setDefaultButton(QMessageBox::Ok);
        msgBox.setEscapeButton(QMessageBox::Ok);
        msgBox.setText("This file cannot be opened.");
        msgBox.exec();
        return;
    }

    QFile openFile(filePath);

    if (openFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        theWorkspace -> addTab(new MonacoEditor(), info.fileName());
        theWorkspace -> setCurrentIndex(theWorkspace -> count() - 1);

        curr = qobject_cast<MonacoEditor*>(theWorkspace -> currentWidget());
        curr -> setPlainText(openFile.readAll()); // resets the modified flag internally -- loading existing content isn't an edit

        curr -> setProperty("filePath", QVariant(filePath));

        wireModifiedIndicator(curr);

        openFile.close();
    }
}


void MainWindow::closeEvent(QCloseEvent *event)
{
    settings.setValue("splitterDimensions", splitter -> saveState());

    QMainWindow::closeEvent(event);
}


bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    QTreeWidget *tree = nullptr;
    if (watched == localFiles -> viewport())
        tree = localFiles;
    else if (watched == cloudFiles -> viewport())
        tree = cloudFiles;

    if (tree)
    {
        if (event -> type() == QEvent::MouseMove)
        {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            bool overItem = tree -> indexAt(mouseEvent -> pos()).isValid();
            tree -> viewport() -> setCursor(overItem ? Qt::PointingHandCursor : Qt::ArrowCursor);
        }
        else if (event -> type() == QEvent::Leave)
            tree -> viewport() -> setCursor(Qt::ArrowCursor);
    }

    return QMainWindow::eventFilter(watched, event);
}


bool MainWindow::cloudFileExists(const QString &fileName) const
{
    for (int i = 0; i < cloudFiles -> topLevelItemCount(); i++)
    {
        if (cloudFiles -> topLevelItem(i) -> text(0) == fileName)
            return true;
    }

    return false;
}


void MainWindow::on_actionUpload_triggered()
{
    curr = qobject_cast<MonacoEditor*>(theWorkspace -> currentWidget());
    QList<QTreeWidgetItem*> ls = localFiles -> selectedItems();
    if (!curr && ls.size() == 0)
    {
        ui -> statusbar -> showMessage("Select a file to upload.", 3000);
        return;
    }
    if (curr && ls.size() == 0)
    {
        if (curr -> property("isCloudFile").toBool())
        {
            storage -> uploadFile(curr -> toPlainText().toUtf8(), curr -> property("cloudFileName").toString());
            return;
        }
        on_actionSave_triggered();

        // This tab isn't tracked as a cloud file (isCloudFile branch above
        // handles that case), so its name might coincidentally match an
        // unrelated cloud file -- cloud files are keyed on filename alone,
        // server-side, so uploading here would silently overwrite it.
        QString fileName = QFileInfo(curr -> property("filePath").toString()).fileName();
        if (cloudFileExists(fileName))
        {
            QMessageBox::StandardButton choice = QMessageBox::question(this, "File Already Exists",
                QString("A cloud file named \"%1\" already exists. Overwrite it?").arg(fileName),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

            if (choice != QMessageBox::Yes)
                return;
        }

        storage -> uploadFile(curr -> toPlainText().toUtf8(), curr -> property("filePath").toString());
        return;
    }

    QStringList unreadableFiles;
    QStringList skippedFiles;

    for (int i = 0; i < ls.size(); i++)
    {
        filePath = ls[i] -> data(0, Qt::UserRole).toString();
        QFile openFile(filePath);

        if (!openFile.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            unreadableFiles << QFileInfo(filePath).fileName();
            continue;
        }

        QString fileName = QFileInfo(filePath).fileName();
        if (cloudFileExists(fileName))
        {
            QMessageBox::StandardButton choice = QMessageBox::question(this, "File Already Exists",
                QString("A cloud file named \"%1\" already exists. Overwrite it?").arg(fileName),
                QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::No);

            // Cancel abandons the rest of the batch outright, including any
            // files not yet reached -- not just the colliding ones.
            if (choice == QMessageBox::Cancel)
                break;

            if (choice == QMessageBox::No)
            {
                skippedFiles << fileName;
                continue;
            }
        }

        storage -> uploadFile(openFile.readAll(), filePath);
    }

    if (!unreadableFiles.isEmpty())
        QMessageBox::warning(this, "Upload Failed",
            "The following files could not be read and were skipped:\n" + unreadableFiles.join("\n"));

    if (!skippedFiles.isEmpty())
        QMessageBox::information(this, "Upload Skipped",
            "The following files already exist in the cloud and were left unchanged:\n" + skippedFiles.join("\n"));
}


void MainWindow::on_actionUpload_File_to_Cloud_triggered()
{
    // The File menu's "Upload File to Cloud" is the toolbar Upload button in
    // every respect -- same target resolution, same collision warning.
    on_actionUpload_triggered();
}


void MainWindow::on_actionDelete_File_from_Cloud_triggered()
{
    // Cloud-side only, deliberately: local files aren't a valid target here
    // (see the point 7 discussion -- unlike upload, delete doesn't try to
    // resolve a matching cloud file by filename from the local tree).
    QList<QTreeWidgetItem*> ls = cloudFiles -> selectedItems();

    QStringList fileIds;
    QStringList fileNames;

    if (ls.size() > 0)
    {
        for (QTreeWidgetItem *item : ls)
        {
            fileIds << item -> data(0, Qt::UserRole).toString();
            fileNames << item -> text(0);
        }
    }
    else
    {
        curr = qobject_cast<MonacoEditor*>(theWorkspace -> currentWidget());
        if (curr && curr -> property("isCloudFile").toBool())
        {
            fileIds << curr -> property("filePath").toString();
            fileNames << curr -> property("cloudFileName").toString();
        }
    }

    if (fileIds.isEmpty())
    {
        ui -> statusbar -> showMessage("Select a cloud file to delete.", 3000);
        return;
    }

    QString message = fileIds.size() == 1
        ? QString("Delete the cloud copy of \"%1\"? This can't be undone.").arg(fileNames.first())
        : QString("Delete these %1 cloud files? This can't be undone.\n\n%2")
              .arg(fileIds.size()).arg(fileNames.join("\n"));

    QMessageBox::StandardButton choice = QMessageBox::question(this, "Delete File from Cloud", message,
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (choice != QMessageBox::Yes)
        return;

    for (int i = 0; i < fileIds.size(); i++)
        storage -> deleteFile(fileIds[i], fileNames[i]);
}


void MainWindow::onDeleteSucceeded(const QString &fileId, const QString &fileName)
{
    Q_UNUSED(fileName);

    // Close every tab open on this file -- normally at most one, but
    // cloudFilesItemClicked() always opens a fresh tab rather than reusing
    // an existing one, so more than one is possible. Iterated backwards so
    // removing a tab doesn't invalidate the indices still to be checked.
    for (int i = theWorkspace -> count() - 1; i >= 0; i--)
    {
        MonacoEditor *tab = qobject_cast<MonacoEditor*>(theWorkspace -> widget(i));
        if (tab && tab -> property("isCloudFile").toBool() && tab -> property("filePath").toString() == fileId)
        {
            theWorkspace -> removeTab(i);
            delete tab;
        }
    }
}


void MainWindow::onDeleteFailed(const QString &fileName, const QString &errorString)
{
    QMessageBox::warning(this, "Delete Failed",
        QString("Could not delete \"%1\" from the cloud:\n%2").arg(fileName, errorString));
}


void MainWindow::onDeleteBatchFinished(int succeededCount)
{
    if (succeededCount <= 0)
        return; // nothing succeeded in this wave -- onDeleteFailed() already covers failures

    ui -> statusbar -> showMessage(
        succeededCount == 1 ? "Deleted 1 file from the cloud." : QString("Deleted %1 files from the cloud.").arg(succeededCount),
        4000);
}


void MainWindow::onSetCloudFiles(const QString &fileName, const QString &cloudFilePath)
{
    cloudFilesStack -> setCurrentWidget(cloudFiles);

    QTreeWidgetItem* child = new QTreeWidgetItem(cloudFiles);
    child -> setText(0, fileName);

    // If this file was just uploaded, show a success tick instead of the
    // usual file icon for this one refresh, then stop tracking it.
    if (recentlyUploadedCloudPaths.remove(cloudFilePath))
        child -> setIcon(0, loadFileTreeIcon(":/icons/Icons/check_circle_24dp_34A853_FILL0_wght400_GRAD0_opsz24.svg"));
    else
        child -> setIcon(0, iconForFileName(fileName));

    child -> setData(0, Qt::UserRole, cloudFilePath);
}


void MainWindow::onUploadSucceeded(const QString &localFilePath, const QString &cloudFilePath)
{
    recentlyUploadedCloudPaths.insert(cloudFilePath);
    // No per-file status message here -- see onUploadBatchFinished(), which
    // reports how many files succeeded once the whole wave settles.

    // Clears the "unsaved changes" dot on whichever open tab this upload was
    // for -- its content just reached the cloud, so it's no longer ahead of
    // what's saved there (same reasoning as on_actionSave_triggered()'s
    // setModified(false), just for the upload path instead of the disk-save
    // path). Matched by property rather than tab identity, since Storage
    // only reports the upload by path/name, not which tab (if any)
    // triggered it -- and which property to match on depends on how the
    // upload started (see on_actionUpload_triggered()): a cloud-tab
    // re-upload passes its cloudFileName (a display name, not a real path)
    // as localFilePath, while a local-file upload (tab or tree selection)
    // passes the real path.
    for (int i = 0; i < theWorkspace -> count(); i++)
    {
        MonacoEditor *tab = qobject_cast<MonacoEditor*>(theWorkspace -> widget(i));
        if (!tab)
            continue;

        bool isCloudTab = tab -> property("isCloudFile").toBool();
        bool matches = isCloudTab
            ? tab -> property("cloudFileName").toString() == localFilePath
            : tab -> property("filePath").toString() == localFilePath;

        if (matches)
            tab -> setModified(false);
    }
}


void MainWindow::onUploadBatchFinished(int succeededCount)
{
    if (succeededCount <= 0)
        return; // nothing succeeded in this wave -- onUploadFailed() already covers failures

    ui -> statusbar -> showMessage(
        succeededCount == 1 ? "Uploaded 1 file" : QString("Uploaded %1 files").arg(succeededCount),
        4000);
}


void MainWindow::onUploadFailed(const QString &localFilePath, const QString &errorString)
{
    QMessageBox::warning(this, "Upload Failed",
        QString("Could not upload \"%1\":\n%2").arg(QFileInfo(localFilePath).fileName(), errorString));
}


void MainWindow::onListFilesFailed(const QString &errorString)
{
    ui -> statusbar -> showMessage("Could not refresh cloud files: " + errorString, 5000);
}


void MainWindow::onDownloadFailed(const QString &errorString, QObject *targetTab)
{
    // The tab opened for this download is now permanently empty and useless
    // -- close it instead of leaving a dead placeholder tab behind. It may
    // already be gone (user closed it while the download was in flight), in
    // which case targetTab is null and there's nothing to clean up.
    MonacoEditor *tab = qobject_cast<MonacoEditor*>(targetTab);
    if (tab)
    {
        int index = theWorkspace -> indexOf(tab);
        if (index != -1)
            theWorkspace -> removeTab(index);
        delete tab;
    }

    QMessageBox::warning(this, "Download Failed", "Could not download the file:\n" + errorString);
}


void MainWindow::onTokenRefreshRequired()
{
    // Shown indefinitely (timeout 0) -- it gets naturally replaced once the
    // retried request resolves, via onUploadSucceeded/onUploadFailed/etc.
    ui -> statusbar -> showMessage("Session expired — refreshing, please wait...", 0);
    loginWindow -> refreshSessionNow();
}


void MainWindow::onIdTokenRefreshed(const QString &idToken)
{
    // Updates the token and transparently redoes everything that was queued
    // waiting on this refresh.
    storage -> resumeAfterTokenRefresh(idToken);
}


void MainWindow::onSessionExpired()
{
    if (handlingSessionExpiry)
        return;
    handlingSessionExpiry = true;

    storage -> abandonPendingRetries();

    ui -> actionUpload -> setEnabled(false);
    ui -> actionLogout -> setEnabled(false);
    ui -> actionUpload_File_to_Cloud -> setEnabled(false);
    ui -> actionDelete_File_from_Cloud -> setEnabled(false);
    cloudFiles -> clear();
    cloudFilesStack -> setCurrentWidget(cloudFilesArea);

    ui -> statusbar -> showMessage("Your session has expired. Please log in again.", 5000);

    loginWindow -> setModal(true);
    loginWindow -> exec();

    handlingSessionExpiry = false;
}

void MainWindow::cloudFilesItemClicked(QTreeWidgetItem *item)
{
    theWorkspace -> addTab(new MonacoEditor(), item->text(0));
    theWorkspace -> setCurrentIndex(theWorkspace -> count() - 1);

    filePath = item -> data(0, Qt::UserRole).toString();

    curr = qobject_cast<MonacoEditor*>(theWorkspace -> currentWidget());
    curr -> setProperty("filePath", QVariant(filePath));

    // filePath here is the backend's numeric file id, not a real path --
    // isCloudFile/cloudFileName are how the upload/close-tab flows tell this
    // tab apart from a tab backed by a real local file.
    curr -> setProperty("isCloudFile", true);
    curr -> setProperty("cloudFileName", item -> text(0));

    wireModifiedIndicator(curr);

    // Pass this exact tab through, rather than relying on "whichever tab is
    // active" when the (asynchronous) download eventually completes -- the
    // user may have switched to a different tab by then.
    storage -> downloadFile(filePath, curr);
}

void MainWindow::onDownloadFile(const QByteArray &response, QObject *targetTab)
{
    MonacoEditor *tab = qobject_cast<MonacoEditor*>(targetTab);
    if (!tab)
        return; // the tab this download was for has since been closed

    tab -> setPlainText(response); // resets the modified flag internally -- loading downloaded content isn't an edit
}


void MainWindow::on_actionClose_Folder_triggered()
{
    QList<QTreeWidgetItem*> ls = localFiles->selectedItems();
    for (int i = 0; i < ls.size(); i++)
    {
        if (ls[i] -> data(0, Qt::UserRole).toString() == "")
            delete ls[i];
    }

    if (localFiles -> topLevelItemCount() == 0)
        localFilesStack -> setCurrentWidget(localFilesArea);
}


void MainWindow::setThemeMode(Theme::Mode mode)
{
    Theme::setMode(mode);
    applyTheme(Theme::isDark());
}


void MainWindow::onColorSchemeChanged()
{
    applyTheme(Theme::isDark());
}


void MainWindow::applyTheme(bool isDark)
{
    currentThemeIsDark = isDark;

    // Toolbar/menu icons are flat black/white glyphs (VS Code's Seti theme
    // has no "system-tinted" notion for these) -- swapped explicitly here
    // rather than via QPalette, which only reaches palette-driven native
    // widget chrome, not icon pixmaps.
    ui -> actionSettings -> setIcon(QIcon(isDark
        ? ":/icons/Icons/settings_24dp_FFFFFF_FILL0_wght400_GRAD0_opsz24.svg"
        : ":/icons/Icons/settings_24dp_000000_FILL0_wght400_GRAD0_opsz24.svg"));
    ui -> actionLogin -> setIcon(QIcon(isDark
        ? ":/icons/Icons/login_24dp_FFFFFF_FILL0_wght400_GRAD0_opsz24.svg"
        : ":/icons/Icons/login_24dp_000000_FILL0_wght400_GRAD0_opsz24.svg"));
    ui -> actionLogout -> setIcon(QIcon(isDark
        ? ":/icons/Icons/logout_24dp_FFFFFF_FILL0_wght400_GRAD0_opsz24.svg"
        : ":/icons/Icons/logout_24dp_000000_FILL0_wght400_GRAD0_opsz24.svg"));
    ui -> actionThe_Vault -> setIcon(QIcon(isDark
        ? ":/icons/Icons/folder_code_24dp_FFFFFF_FILL0_wght400_GRAD0_opsz24.svg"
        : ":/icons/Icons/folder_code_24dp_000000_FILL0_wght400_GRAD0_opsz24.svg"));
    ui -> actionUpload -> setIcon(QIcon(isDark
        ? ":/icons/Icons/cloud_upload_24dp_FFFFFF_FILL0_wght400_GRAD0_opsz24.svg"
        : ":/icons/Icons/cloud_upload_24dp_000000_FILL0_wght400_GRAD0_opsz24.svg"));

    // File-tree hover overlay and the "no folder/no files open" placeholder
    // text -- both were hardcoded white-on-dark, unreadable once the
    // background actually goes light.
    //
    // The ":selected" rule below is what keeps a selected row's highlight
    // from being clobbered by the ":hover" rule above the instant the mouse
    // moves over it: putting any stylesheet rule on QTreeWidget::item
    // switches Qt to CSS box-model painting for every pseudo-state of that
    // item, selected+hovered included, and with only ":hover" defined that
    // combined state fell through to the hover overlay instead of staying
    // selected. ":hover" and ":selected" are equal-specificity selectors
    // (one pseudo-class each on the same subcontrol), so on a combined
    // selected+hovered row Qt's cascade breaks the tie in favor of whichever
    // was declared last -- ":selected" is deliberately second here so it
    // wins that tie, without a separate ":selected:hover" rule.
    //
    // Light mode uses fixed grays (VS Code's own light-theme file-explorer
    // hover/selected colors, not macOS's native blue) rather than
    // "palette(highlight)" -- picked deliberately, per the user, over
    // deferring to the native highlight color, since forcing the app's
    // color scheme (Theme::setMode(Light), see theme.h) doesn't fully
    // replicate native rendering anyway (confirmed: explicit Light-mode
    // selection looked visibly different from System-mode-while-the-OS-is-
    // light, even though both are "light" and should look the same). Fixed
    // colors sidestep that mismatch entirely, since System-with-a-light-OS
    // and explicit Light both collapse to isDark == false here regardless.
    // Dark mode keeps deferring to the native highlight color.
    //
    // "color: palette(text);" on both rules is deliberate -- only the
    // background should ever change on hover/selection, per the user; file
    // names should read identically to a plain unselected row. Without it,
    // Qt's item delegate paints selected/hovered text using the palette's
    // "inactive highlighted text" color group, which comes out as a washed-
    // out gray rather than the normal text color. See loadFileTreeIcon() in
    // mainwindow.h for the equivalent fix on the icon side -- Qt dims icon
    // pixmaps for a selected row the same way unless told not to.
    QString fileTreeHoverStyle = isDark
        ? "QTreeWidget::item:hover { background: rgba(255,255,255,25); color: palette(text); }"
          "QTreeWidget::item:selected { background: palette(highlight); color: palette(text); }"
        : "QTreeWidget::item:hover { background: #E6E6E9; color: palette(text); }"
          "QTreeWidget::item:selected { background: #D7D7D9; color: palette(text); }";
    localFiles -> setStyleSheet(fileTreeHoverStyle);
    cloudFiles -> setStyleSheet(fileTreeHoverStyle);

    QString placeholderStyle = QString("color: rgba(%1,%1,%1,140);").arg(isDark ? 255 : 0);
    localFilesArea -> setStyleSheet(placeholderStyle);
    cloudFilesArea -> setStyleSheet(placeholderStyle);

    refreshTreeIcons();

    // Keeps every already-open Monaco tab in lockstep -- a half-and-half
    // state (light chrome, dark editor or vice versa) is a regression, not
    // a valid in-between.
    for (int i = 0; i < theWorkspace -> count(); ++i)
    {
        MonacoEditor *tab = qobject_cast<MonacoEditor*>(theWorkspace -> widget(i));
        if (tab)
            tab -> applyTheme(isDark);
    }
}


void MainWindow::refreshTreeIcons()
{
    // Only the local files tree is walked here -- see the reasoning on the
    // refreshTreeIcons() declaration in mainwindow.h.
    for (int i = 0; i < localFiles -> topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *root = localFiles -> topLevelItem(i);
        for (int j = 0; j < root -> childCount(); ++j)
        {
            QTreeWidgetItem *child = root -> child(j);
            QString path = child -> data(0, Qt::UserRole).toString();
            if (path.isEmpty())
                continue;

            if (QFileInfo(path).isExecutable())
                child -> setIcon(0, iconForExecutable());
            else
                child -> setIcon(0, iconForFileName(child -> text(0)));
        }
    }
}


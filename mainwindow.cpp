#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "syntaxhighlighter.h"
#include "codeeditor.h"
#include "terminal.h"
#include <QCloseEvent>
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
#include <QTextBlock>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

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
    ui -> toolBar -> addAction(ui -> actionSettings);
    ui -> actionUpload -> setEnabled(false);

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


    localFilesArea = new QLabel("Local Files' Area");
    localFilesStack -> addWidget(localFilesArea);
    localFilesArea->setAlignment(Qt::AlignCenter);
    localFilesArea->setWordWrap(true);
    localFilesArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    localFilesArea->setStyleSheet("color: rgba(255,255,255,140);");

    cloudFilesArea = new QLabel("Cloud Files' Area");
    cloudFilesStack -> addWidget(cloudFilesArea);
    cloudFilesArea->setAlignment(Qt::AlignCenter);
    cloudFilesArea->setWordWrap(true);
    cloudFilesArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    cloudFilesArea->setStyleSheet("color: rgba(255,255,255,140);");

    localFilesStack -> setCurrentWidget(localFilesArea);
    cloudFilesStack -> setCurrentWidget(cloudFilesArea);

    connect(localFiles, &QTreeWidget::itemDoubleClicked, this, &MainWindow::localFilesItemClicked);
    connect(cloudFiles, &QTreeWidget::itemDoubleClicked, this, &MainWindow::cloudFilesItemClicked);
    connect(storage, &Storage::cloudFilesCleared, cloudFiles, &QTreeWidget::clear);
    connect(storage, &Storage::setCloudFiles, this, &MainWindow::onSetCloudFiles);
    connect(storage, &Storage::setDownloadFile, this, &MainWindow::onDownloadFile);
    connect(storage, &Storage::uploadSucceeded, this, &MainWindow::onUploadSucceeded);
    connect(storage, &Storage::uploadFailed, this, &MainWindow::onUploadFailed);
    connect(storage, &Storage::listFilesFailed, this, &MainWindow::onListFilesFailed);
    connect(storage, &Storage::downloadFailed, this, &MainWindow::onDownloadFailed);
    connect(storage, &Storage::tokenRefreshRequired, this, &MainWindow::onTokenRefreshRequired);
    connect(storage, &Storage::backendLoginSucceeded, this, &MainWindow::onBackendLoginSucceeded);
    connect(storage, &Storage::backendLoginFailed, this, &MainWindow::onBackendLoginFailed);
    connect(loginWindow, &LoginWindow::enableActionUpload, this, &MainWindow::onEnableActionUpload);
    connect(loginWindow, &LoginWindow::idTokenRefreshed, this, &MainWindow::onIdTokenRefreshed);
    connect(loginWindow, &LoginWindow::sessionExpired, this, &MainWindow::onSessionExpired);

    if (!settings.contains("splitterDimensions"))
        splitter -> setSizes({100, 100});

    // Not letting the workspace completely collapse
    splitter -> setCollapsible(1, false);
    // Setting minimum width for the workspace
    theWorkspace -> setMinimumWidth(100);

    // Remembering the previous sizes of each layout
    splitter -> restoreState(settings.value("splitterDimensions").toByteArray());
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
    curr = qobject_cast<QPlainTextEdit*>(theWorkspace -> widget(index));

    QMessageBox msgBox(this);
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Save);
    msgBox.setEscapeButton(QMessageBox::Cancel);

    filePath = curr -> property("filePath").toString();

    if (curr -> property("isCloudFile").toBool())
    {
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
        closeFile.open(QIODevice::ReadOnly | QIODevice::Text);

        if (curr -> toPlainText() != closeFile.readAll())
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


void MainWindow::wireModifiedIndicator(QPlainTextEdit *tab)
{
    connect(tab -> document(), &QTextDocument::modificationChanged, this, [this, tab](bool modified) {
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


void MainWindow::on_actionNew_File_triggered()
{
    theWorkspace -> addTab(new CodeEditor(), QString("Tab %0").arg(theWorkspace -> count() + 1));
    curr = qobject_cast<QPlainTextEdit*>(theWorkspace -> currentWidget());
    curr -> setProperty("filePath", QVariant(""));

    theWorkspace -> setCurrentIndex(theWorkspace -> count() - 1);

    new SyntaxHighlighter(curr -> document());
    wireModifiedIndicator(curr);
}


void MainWindow::on_actionOpen_triggered()
{
    filePath = QFileDialog::getOpenFileName(this, "", projectFolderPath, "*.cpp");
    QFile openFile(filePath);

    if (filePath.isEmpty())
        return;

    openFile.open(QIODevice::ReadOnly | QIODevice::Text);

    info = QFileInfo(filePath);
    theWorkspace->addTab(new CodeEditor(), info.fileName());
    theWorkspace -> setCurrentIndex(theWorkspace -> count() - 1);

    curr = qobject_cast<QPlainTextEdit*>(theWorkspace -> currentWidget());
    curr -> setPlainText(openFile.readAll());
    curr -> document() -> setModified(false); // loading existing content isn't an edit

    curr -> setProperty("filePath", QVariant(filePath));

    new SyntaxHighlighter(curr -> document());
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
        if (info.suffix() == "cpp")
            child -> setIcon(0, QIcon(":/icons/Icons/icons8-c++.svg"));
        else if (info.isExecutable())
            child -> setIcon(0, QIcon(":/icons/Icons/output_circle_24dp_FFFFFF_FILL0_wght400_GRAD0_opsz24.svg"));

        child -> setData(0, Qt::UserRole, it.filePath());
    }
}


void MainWindow::on_actionSave_triggered()
{
    if (theWorkspace -> currentIndex() == -1)
        return;

    curr = qobject_cast<QPlainTextEdit*>(theWorkspace -> currentWidget());

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
        filePath = QFileDialog::getSaveFileName(this, "", projectFolderPath);

        if (filePath.isEmpty())
            return;

        if (!filePath.endsWith(".cpp"))
            filePath += ".cpp";

        curr -> setProperty("filePath", QVariant(filePath));

        info = QFileInfo(filePath);
        theWorkspace -> setTabText(theWorkspace -> currentIndex(), info.fileName());

        // Deliberately falls through to the write below instead of
        // returning here -- this used to return right after picking a
        // filename, which meant the very first save of a new file renamed
        // the tab but never actually wrote it to disk.
    }


    QFile saveFile(filePath);

    saveFile.open(QIODevice::WriteOnly | QIODevice::Text);

    QTextStream out(&saveFile);
    out << curr -> toPlainText();

    saveFile.flush();
    saveFile.close();

    curr -> document() -> setModified(false);
}


void MainWindow::on_actionUndo_triggered()
{
    if (theWorkspace -> currentIndex() != -1)
        qobject_cast<QPlainTextEdit*>(theWorkspace -> currentWidget()) -> undo();
}


void MainWindow::on_actionRedo_triggered()
{
    if (theWorkspace -> currentIndex() != -1)
        qobject_cast<QPlainTextEdit*>(theWorkspace -> currentWidget()) -> redo();
}


void MainWindow::on_actionCut_triggered()
{
    if (theWorkspace -> currentIndex() != -1)
        qobject_cast<QPlainTextEdit*>(theWorkspace -> currentWidget()) -> cut();
}


void MainWindow::on_actionCopy_triggered()
{
    if (theWorkspace -> currentIndex() != -1)
        qobject_cast<QPlainTextEdit*>(theWorkspace -> currentWidget()) -> copy();
}


void MainWindow::on_actionPaste_triggered()
{
    if (theWorkspace -> currentIndex() != -1)
        qobject_cast<QPlainTextEdit*>(theWorkspace -> currentWidget()) -> paste();
}


void MainWindow::on_actionToggle_Comment_triggered()
{
    if (theWorkspace -> currentIndex() == -1)
        return;

    QPlainTextEdit *editor = qobject_cast<QPlainTextEdit*>(theWorkspace -> currentWidget());
    QTextDocument *doc = editor -> document();
    QTextCursor cursor = editor -> textCursor();

    QTextCursor startCursor(doc);
    startCursor.setPosition(cursor.selectionStart());
    QTextCursor endCursor(doc);
    endCursor.setPosition(cursor.selectionEnd());

    int firstBlock = startCursor.blockNumber();
    int lastBlock = endCursor.blockNumber();

    // A selection ending exactly at the start of a line shouldn't pull that
    // line into the toggle -- matches how most editors treat a trailing
    // newline caught by a drag-selection.
    if (lastBlock > firstBlock && endCursor.atBlockStart())
        --lastBlock;

    // If every non-blank line in range is already commented, this toggles
    // to uncommented; otherwise it comments every line that isn't already.
    bool allCommented = true;
    for (int b = firstBlock; b <= lastBlock; ++b)
    {
        QString text = doc -> findBlockByNumber(b).text();
        if (!text.trimmed().isEmpty() && !text.trimmed().startsWith("//"))
        {
            allCommented = false;
            break;
        }
    }

    cursor.beginEditBlock();
    for (int b = firstBlock; b <= lastBlock; ++b)
    {
        QTextBlock block = doc -> findBlockByNumber(b);
        QString text = block.text();
        if (text.trimmed().isEmpty())
            continue;

        int firstNonSpace = 0;
        while (firstNonSpace < text.length() && (text[firstNonSpace] == ' ' || text[firstNonSpace] == '\t'))
            ++firstNonSpace;

        QTextCursor lineCursor(block);

        if (allCommented)
        {
            int slashPos = text.indexOf("//", firstNonSpace);
            int removeLength = (text.mid(slashPos + 2, 1) == " ") ? 3 : 2;
            lineCursor.setPosition(block.position() + slashPos);
            lineCursor.setPosition(block.position() + slashPos + removeLength, QTextCursor::KeepAnchor);
            lineCursor.removeSelectedText();
        }
        else
        {
            lineCursor.setPosition(block.position() + firstNonSpace);
            lineCursor.insertText("// ");
        }
    }
    cursor.endEditBlock();
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

void MainWindow::onEnableActionUpload(bool flag, const QString& idToken, const QString& uid)
{
    Q_UNUSED(uid); // the backend derives identity from the token itself, server-side

    ui -> actionUpload -> setEnabled(flag);
    storage -> setIdToken(idToken);

    // Establishes/refreshes the users row for this session before anything
    // else is allowed to touch /files -- see onBackendLoginSucceeded().
    storage -> loginToBackend();
}


void MainWindow::onBackendLoginSucceeded()
{
    storage -> listFiles();
}


void MainWindow::onBackendLoginFailed(const QString &errorString)
{
    ui -> actionUpload -> setEnabled(false);
    QMessageBox::warning(this, "Backend Unavailable",
        "Could not reach the file-storage backend:\n" + errorString +
        "\n\nCheck the backend URL under Settings.");
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
        return;
    on_actionSave_triggered();
    Terminal* myTerminal = new Terminal(this, this);
    myTerminal -> runFile();
}


void MainWindow::localFilesItemClicked(QTreeWidgetItem* item)
{
    filePath = item -> data(0, Qt::UserRole).toString();
    if (filePath.contains(".cpp") == false)
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
        theWorkspace -> addTab(new CodeEditor(), info.fileName());
        theWorkspace -> setCurrentIndex(theWorkspace -> count() - 1);

        curr = qobject_cast<QPlainTextEdit*>(theWorkspace -> currentWidget());
        curr -> setPlainText(openFile.readAll());
        curr -> document() -> setModified(false); // loading existing content isn't an edit

        curr -> setProperty("filePath", QVariant(filePath));

        new SyntaxHighlighter(curr -> document());
        wireModifiedIndicator(curr);

        openFile.close();
    }
}


void MainWindow::closeEvent(QCloseEvent *event)
{
    settings.setValue("splitterDimensions", splitter -> saveState());

    QMainWindow::closeEvent(event);
}


void MainWindow::on_actionUpload_triggered()
{
    curr = qobject_cast<QPlainTextEdit*>(theWorkspace -> currentWidget());
    QList<QTreeWidgetItem*> ls = localFiles -> selectedItems();
    if (!curr && ls.size() == 0)
        return;
    if (curr && ls.size() == 0)
    {
        if (curr -> property("isCloudFile").toBool())
        {
            storage -> uploadFile(curr -> toPlainText().toUtf8(), curr -> property("cloudFileName").toString());
            return;
        }
        on_actionSave_triggered();
        storage -> uploadFile(curr -> toPlainText().toUtf8(), curr -> property("filePath").toString());
        return;
    }

    QStringList unreadableFiles;

    for (int i = 0; i < ls.size(); i++)
    {
        filePath = ls[i] -> data(0, Qt::UserRole).toString();
        QFile openFile(filePath);

        if (!openFile.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            unreadableFiles << QFileInfo(filePath).fileName();
            continue;
        }

        storage -> uploadFile(openFile.readAll(), filePath);
    }

    if (!unreadableFiles.isEmpty())
        QMessageBox::warning(this, "Upload Failed",
            "The following files could not be read and were skipped:\n" + unreadableFiles.join("\n"));
}

void MainWindow::onSetCloudFiles(const QString &fileName, const QString &cloudFilePath)
{
    cloudFilesStack -> setCurrentWidget(cloudFiles);

    QTreeWidgetItem* child = new QTreeWidgetItem(cloudFiles);
    child -> setText(0, fileName);

    // If this file was just uploaded, show a success tick instead of the
    // usual file icon for this one refresh, then stop tracking it.
    if (recentlyUploadedCloudPaths.remove(cloudFilePath))
        child -> setIcon(0, QIcon(":/icons/Icons/check_circle_24dp_34A853_FILL0_wght400_GRAD0_opsz24.svg"));
    else
        child -> setIcon(0, QIcon(":/icons/Icons/icons8-c++.svg"));

    child -> setData(0, Qt::UserRole, cloudFilePath);
}


void MainWindow::onUploadSucceeded(const QString &localFilePath, const QString &cloudFilePath)
{
    recentlyUploadedCloudPaths.insert(cloudFilePath);
    ui -> statusbar -> showMessage(QString("Uploaded \"%1\"").arg(QFileInfo(localFilePath).fileName()), 4000);
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
    QPlainTextEdit *tab = qobject_cast<QPlainTextEdit*>(targetTab);
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
    cloudFiles -> clear();
    cloudFilesStack -> setCurrentWidget(cloudFilesArea);

    ui -> statusbar -> showMessage("Your session has expired. Please log in again.", 5000);

    loginWindow -> setModal(true);
    loginWindow -> exec();

    handlingSessionExpiry = false;
}

void MainWindow::cloudFilesItemClicked(QTreeWidgetItem *item)
{
    theWorkspace -> addTab(new CodeEditor(), item->text(0));
    theWorkspace -> setCurrentIndex(theWorkspace -> count() - 1);

    filePath = item -> data(0, Qt::UserRole).toString();

    curr = qobject_cast<QPlainTextEdit*>(theWorkspace -> currentWidget());
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
    QPlainTextEdit *tab = qobject_cast<QPlainTextEdit*>(targetTab);
    if (!tab)
        return; // the tab this download was for has since been closed

    tab -> setPlainText(response);
    tab -> document() -> setModified(false); // loading downloaded content isn't an edit

    new SyntaxHighlighter(tab -> document());
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


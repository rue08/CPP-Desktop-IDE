#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "syntaxhighlighter.h"
#include "terminal.h"
#include <QCloseEvent>
#include <QMessageBox>
#include <QPushButton>
#include <QDir>
#include <QDirIterator>
#include <QStackedWidget>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    projectFolderPath = QStringLiteral(PROJECT_ROOT_DIR);

    storage = new Storage(this);
    loginWindow = new LoginWindow(this, this);

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
    connect(storage, &Storage::setCloudFiles, this, &MainWindow::onSetCloudFiles);
    connect(storage, &Storage::setDownloadFile, this, &MainWindow::onDownloadFile);
    connect(loginWindow, &LoginWindow::enableActionUpload, this, &MainWindow::onEnableActionUpload);

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

    if (filePath.contains("users", Qt::CaseSensitive))
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
        {
            int temp = 1;
            storage -> setPendingTasks(temp);
            storage -> uploadFile(curr -> toPlainText().toUtf8(), filePath);
        }
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

    if (reply == 0x00800000)
    {
        theWorkspace->removeTab(index);
        delete curr;
        return;
    }
    else if (reply == 0x00400000)
        return;
    else if (reply == 0x00000800)
    {
        on_actionSave_triggered();
        if (!filePath.isEmpty())
        {
            theWorkspace->removeTab(index);
            delete curr;
        }
    }
}


void MainWindow::on_actionNew_File_triggered()
{
    theWorkspace -> addTab(new QPlainTextEdit(), QString("Tab %0").arg(theWorkspace -> count() + 1));
    curr = qobject_cast<QPlainTextEdit*>(theWorkspace -> currentWidget());
    curr -> setProperty("filePath", QVariant(""));

    QFontMetricsF fontMetrics(curr->font());
    curr->setTabStopDistance(fontMetrics.horizontalAdvance(' ') * 4);

    theWorkspace -> setCurrentIndex(theWorkspace -> count() - 1);

    new SyntaxHighlighter(curr -> document());
}


void MainWindow::on_actionOpen_triggered()
{
    filePath = QFileDialog::getOpenFileName(this, "", projectFolderPath, "*.cpp");
    QFile openFile(filePath);

    if (filePath.isEmpty())
        return;

    openFile.open(QIODevice::ReadOnly | QIODevice::Text);

    info = QFileInfo(filePath);
    theWorkspace->addTab(new QPlainTextEdit(), info.fileName());
    theWorkspace -> setCurrentIndex(theWorkspace -> count() - 1);

    curr = qobject_cast<QPlainTextEdit*>(theWorkspace -> currentWidget());
    curr -> setPlainText(openFile.readAll());

    curr -> setProperty("filePath", QVariant(filePath));

    QFontMetricsF fontMetrics(curr->font());
    curr->setTabStopDistance(fontMetrics.horizontalAdvance(' ') * 4);

    new SyntaxHighlighter(curr -> document());

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

        return;
    }


    QFile saveFile(filePath);

    saveFile.open(QIODevice::WriteOnly | QIODevice::Text);

    QTextStream out(&saveFile);
    out << curr -> toPlainText();

    saveFile.flush();
    saveFile.close();
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
    ui -> actionUpload -> setEnabled(flag);
    storage -> setIdToken(idToken);
    storage -> setUid(uid);
    cloudFiles -> clear();
    storage -> listFiles();
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
        theWorkspace -> addTab(new QPlainTextEdit(), info.fileName());
        theWorkspace -> setCurrentIndex(theWorkspace -> count() - 1);

        curr = qobject_cast<QPlainTextEdit*>(theWorkspace -> currentWidget());
        curr -> setPlainText(openFile.readAll());

        curr -> setProperty("filePath", QVariant(filePath));

        QFontMetricsF fontMetrics(curr->font());
        curr->setTabStopDistance(fontMetrics.horizontalAdvance(' ') * 4);

        new SyntaxHighlighter(curr -> document());

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
        cloudFiles->clear();
        if (curr -> property("filePath").toString().contains("users", Qt::CaseSensitive))
        {
            int temp = 1;
            storage -> setPendingTasks(temp);
            storage -> uploadFile(curr -> toPlainText().toUtf8(), curr -> property("filePath").toString());
            return;
        }
        on_actionSave_triggered();
        storage -> uploadFile(curr -> toPlainText().toUtf8(), curr -> property("filePath").toString());
        return;
    }

    for (int i = 0; i < ls.size(); i++)
    {
        if (i == 0)
        {
            cloudFiles->clear();
            storage -> setPendingTasks(ls.size());
        }

        if (curr)
            storage -> uploadFile(curr -> toPlainText().toUtf8(), ls[i] -> data(0, Qt::UserRole).toString());
    }
}

void MainWindow::onSetCloudFiles(const QString &fileName, const QString &cloudFilePath)
{
    cloudFilesStack -> setCurrentWidget(cloudFiles);

    QTreeWidgetItem* child = new QTreeWidgetItem(cloudFiles);
    child -> setText(0, fileName);
    child -> setIcon(0, QIcon(":/icons/Icons/icons8-c++.svg"));
    child -> setData(0, Qt::UserRole, cloudFilePath);
}

void MainWindow::cloudFilesItemClicked(QTreeWidgetItem *item)
{
    theWorkspace -> addTab(new QPlainTextEdit(), item->text(0));
    theWorkspace -> setCurrentIndex(theWorkspace -> count() - 1);

    filePath = item -> data(0, Qt::UserRole).toString();

    curr = qobject_cast<QPlainTextEdit*>(theWorkspace -> currentWidget());
    curr -> setProperty("filePath", QVariant(filePath));

    storage -> downloadFile(filePath);
}

void MainWindow::onDownloadFile(const QByteArray &response)
{
    curr = qobject_cast<QPlainTextEdit*>(theWorkspace -> currentWidget());
    curr -> setPlainText(response);

    QFontMetricsF fontMetrics(curr->font());
    curr->setTabStopDistance(fontMetrics.horizontalAdvance(' ') * 4);

    new SyntaxHighlighter(curr -> document());
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


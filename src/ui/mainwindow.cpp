#include <QtWidgets>

#include "mainwindow.h"
#include "qaboutdialog.h"
#include "src/gbl/gbl_version.h"
#include "src/gbl/gbl_historymodel.h"
#include "src/gbl/gbl_filemodel.h"
#include "src/ui/historyview.h"
#include "src/ui/fileview.h"
#include "diffview.h"
#include "clonedialog.h"
#include "src/gbl/gbl_storage.h"
#include "commitdetail.h"
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QDir>
#include <QFileInfo>
#include <QSplitter>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_qpRepo = NULL;
    m_pHistModel = NULL;
    m_pHistView = NULL;
    m_pNetAM = NULL;
    m_pNetCache = NULL;

    init();
}

MainWindow::~MainWindow()
{
    if (m_qpRepo)
    {
        delete m_qpRepo;
    }

    if (m_pNetAM) { delete m_pNetAM; }
    //if (m_pNetCache) { delete m_pNetCache;}
    cleanupDocks();
}

void MainWindow::cleanupDocks()
{
    QMapIterator<QString, QDockWidget*> i(m_docks);
    while (i.hasNext()) {
        i.next();
        QDockWidget *pDock = i.value();
        delete pDock;
    }

    m_docks.clear();

}

void MainWindow::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event);

    writeSettings();
}

void MainWindow::about()
{
   /*QMessageBox::about(this, tr("About GitBusyLivin"),
            tr("Hope is a good thing, maybe the best of things, and no good thing ever dies."));*/
   QAboutDialog about(this);
   about.exec();
}

void MainWindow::clone()
{
    CloneDialog cloneDlg(this);
    if (cloneDlg.exec() == QDialog::Accepted)
    {
        QString src = cloneDlg.getSource();
        QString dst = cloneDlg.getDestination();

        if (m_qpRepo->clone_repo(src, dst))
        {
            setupRepoUI(dst);
        }
        else
        {
            QMessageBox::warning(this, tr("Clone Error"), m_qpRepo->get_error_msg());
        }
    }
}

void MainWindow::new_local_repo()
{
    QString dirName = QFileDialog::getExistingDirectory(this, tr("Open Directory"),QString(), QFileDialog::ShowDirsOnly);
    if (!dirName.isEmpty())
    {
        if (!m_qpRepo->init_repo(dirName))
        {
            QMessageBox::warning(this, tr("Creation Error"), m_qpRepo->get_error_msg());
        }
    }
}

void MainWindow::new_network_repo()
{
    QString dirName = QFileDialog::getExistingDirectory(this, tr("Open Directory"),QString(), QFileDialog::ShowDirsOnly);
    if (!dirName.isEmpty())
    {
        if (!m_qpRepo->init_repo(dirName, true))
        {
            QMessageBox::warning(this, tr("Creation Error"), m_qpRepo->get_error_msg());
        }
    }
}

void MainWindow::open()
{
    QString dirName = QFileDialog::getExistingDirectory(this);
    if (!dirName.isEmpty())
    {
        if (m_qpRepo->open_repo(dirName))
        {
            MainWindow::prependToRecentRepos(dirName);
            setupRepoUI(dirName);
        }
        else
        {
            QMessageBox::warning(this, tr("Open Error"), m_qpRepo->get_error_msg());
        }
    }
}

void MainWindow::openRecentRepo()
{
    if (const QAction *action = qobject_cast<const QAction *>(sender()))
    {
        QString dirName = action->data().toString();
        if (m_qpRepo->open_repo(dirName))
        {
            MainWindow::prependToRecentRepos(dirName);
            setupRepoUI(dirName);
        }
        else
        {
            QMessageBox::warning(this, tr("Open Error"), m_qpRepo->get_error_msg());
        }

    }
}

void MainWindow::setupRepoUI(QString repoDir)
{
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    QFileInfo fi(repoDir);
    QString title(GBL_APP_NAME);
    QTextStream(&title) << " - " << fi.fileName();
    setWindowTitle(title);

    GBL_History_Array *pHistArr;
    m_qpRepo->get_history(&pHistArr);

    m_pHistModel->setModelData(pHistArr);
    m_pHistView->reset();
    QDockWidget *pDock = m_docks["history_details"];
    QSplitter *pSplit = (QSplitter*)pDock->widget();
    CommitDetailScrollArea *pDetailSA = (CommitDetailScrollArea*)pSplit->widget(0);
    pDetailSA->reset();
    FileView *pView = (FileView*)pSplit->widget(1);
    pView->reset();
    GBL_FileModel *pMod = (GBL_FileModel*)pView->model();
    pMod->cleanFileArray();

    pDock = m_docks["file_diff"];
    DiffView *pDV = (DiffView*)pDock->widget();
    pDV->reset();

    QApplication::restoreOverrideCursor();

}

void MainWindow::historySelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    Q_UNUSED(deselected);
    QModelIndexList mil = selected.indexes();

    //single row selection
    if (mil.count() > 0)
    {
        QModelIndex mi = mil.at(0);
        int row = mi.row();
        GBL_History_Item *pHistItem = m_pHistModel->getHistoryItemAt(row);
        if (pHistItem)
        {
            QDockWidget *pDock = m_docks["history_details"];
            QSplitter *pSplit = (QSplitter*)pDock->widget();
            FileView *pView = (FileView*)pSplit->widget(1);
            CommitDetailScrollArea *pDetail = (CommitDetailScrollArea*)pSplit->widget(0);
            pDetail->setDetails(pHistItem, m_pHistModel->getAvatar(pHistItem->hist_author_email));
            pView->reset();
            GBL_FileModel *pMod = (GBL_FileModel*)pView->model();
            pMod->cleanFileArray();
            pMod->setHistoryItem(pHistItem);
            pDock = m_docks["file_diff"];
            DiffView *pDV = (DiffView*)pDock->widget();
            pDV->reset();

            //m_qpRepo->get_tree_from_commit_oid(pHistItem->hist_oid, pMod);
            m_qpRepo->get_commit_to_parent_diff_files(pHistItem->hist_oid, pMod);
        }
    }
}

void MainWindow::historyFileSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    Q_UNUSED(deselected);
    QModelIndexList mil = selected.indexes();

    //single row selection
    if (mil.count() > 0)
    {
        QModelIndex mi = mil.at(0);
        int row = mi.row();
        QDockWidget *pDock = m_docks["history_details"];
        QSplitter *pSplit = (QSplitter*)pDock->widget();
        FileView *pView = (FileView*)pSplit->widget(1);
        GBL_FileModel *pFileMod = (GBL_FileModel*)pView->model();
        GBL_File_Item *pFileItem = pFileMod->getFileItemAt(row);
        if (pFileItem)
        {
            QString path;
            QString sub;
            if (pFileItem->sub_dir != '.')
            {
                sub = pFileItem->sub_dir;
                sub += '/';
            }
            QTextStream(&path) << sub << pFileItem->file_name;
            QByteArray baPath = path.toUtf8();
            GBL_History_Item *pHistItem = pFileMod->getHistoryItem();
            QDockWidget *pDock = m_docks["file_diff"];
            DiffView *pDV = (DiffView*)pDock->widget();
            pDV->reset();

            if (m_qpRepo->get_commit_to_parent_diff_lines(pHistItem->hist_oid, this, baPath.data()))
            {
                pDV->setDiffFromLines(pFileItem);
            }

        }
    }
}

void MainWindow::addToDiffView(GBL_Line_Item *pLineItem)
{
    QDockWidget *pDock = m_docks["file_diff"];
    DiffView *pDV = (DiffView*)pDock->widget();

    pDV->addLine(pLineItem);
}

void MainWindow::preferences()
{
    QMessageBox::about(this, tr("D'oh"),
             tr("Under Construction"));
}

void MainWindow::toggleStatusBar()
{
    if (statusBar()->isVisible())
    {
        statusBar()->hide();
    }
    else
    {
       statusBar()->show();
    }
}

void MainWindow::toggleToolBar()
{
    if (m_pToolBar->isVisible())
    {
        m_pToolBar->hide();
    }
    else
    {
        m_pToolBar->show();
    }
}

void MainWindow::init()
{
    m_qpRepo = new GBL_Repository();
    m_pToolBar = addToolBar(tr(GBL_APP_NAME));
    m_pToolBar->setObjectName("MainWindow/Toolbar");
    createActions();
    createDocks();
    setWindowTitle(tr(GBL_APP_NAME));
    statusBar()->showMessage(tr("Ready"));
    readSettings();
}

void MainWindow::createActions()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    QMenu *newMenu = fileMenu->addMenu(tr("&New"));
    newMenu->addAction(tr("&Local Repository..."), this, &MainWindow::new_local_repo);
    newMenu->addAction(tr("&Network Repository..."), this, &MainWindow::new_network_repo);
    fileMenu->addAction(tr("&Clone..."), this, &MainWindow::clone);
    fileMenu->addAction(tr("&Open..."), this, &MainWindow::open);
    fileMenu->addSeparator();

    QMenu *recentMenu = fileMenu->addMenu(tr("Recent"));
    connect(recentMenu, &QMenu::aboutToShow, this, &MainWindow::updateRecentRepoActions);
    m_pRecentRepoSubMenuAct = recentMenu->menuAction();

    for (int i = 0; i < MaxRecentRepos; ++i) {
        m_pRecentRepoActs[i] = recentMenu->addAction(QString(), this, &MainWindow::openRecentRepo);
        m_pRecentRepoActs[i]->setVisible(false);
    }

    m_pRecentRepoSeparator = fileMenu->addSeparator();

    setRecentReposVisible(MainWindow::hasRecentRepos());


 #ifdef Q_OS_WIN
    QString sQuit = tr("&Exit");
 #else
    QString sQuit = tr("&Quit");
 #endif
    QAction *quitAct = fileMenu->addAction(sQuit, this, &QWidget::close);
    quitAct->setShortcuts(QKeySequence::Quit);
    quitAct->setStatusTip(tr("Quit the application"));

    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(tr("&Preferences..."), this, &MainWindow::preferences);

    m_pViewMenu = menuBar()->addMenu(tr("&View"));
    QAction *tbAct = m_pViewMenu->addAction(tr("&Toolbar"));
    QAction *sbAct = m_pViewMenu->addAction(tr("&Statusbar"));
    tbAct->setCheckable(true);
    tbAct->setChecked(true);
    sbAct->setCheckable(true);
    sbAct->setChecked(true);
    connect(tbAct, &QAction::toggled, this, &MainWindow::toggleToolBar);
    connect(sbAct, &QAction::toggled, this, &MainWindow::toggleStatusBar);
    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(tr("&About GitBusyLivin"), this, &MainWindow::about);
}

void MainWindow::createDocks()
{
    m_pHistModel = new GBL_HistoryModel(NULL, this);
    m_pHistView = new HistoryView(this);
    m_pHistView->setModel(m_pHistModel);
    m_pHistView->verticalHeader()->hide();
    //m_pHistView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_pHistView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_pHistView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_pHistView->setShowGrid(false);
    m_pHistView->setAlternatingRowColors(true);
    connect(m_pHistView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::historySelectionChanged);
    setCentralWidget(m_pHistView);

    //setup history details dock
    QDockWidget *pDock = new QDockWidget(tr("History - Details"), this);
    QSplitter *pDetailSplit = new QSplitter(Qt::Vertical, pDock);
    pDetailSplit->setFrameStyle(QFrame::StyledPanel);
    CommitDetailScrollArea *pScroll = new CommitDetailScrollArea(pDetailSplit);
    FileView *pView = new FileView(pDetailSplit);
    pDetailSplit->addWidget(pScroll);
    pDetailSplit->addWidget(pView);
    pDock->setWidget(pDetailSplit);
    pView->setModel(new GBL_FileModel(pView));
    connect(pView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::historyFileSelectionChanged);
    m_docks["history_details"] = pDock;
    pDock->setObjectName("MainWindow/HistoryDetails");
    addDockWidget(Qt::BottomDockWidgetArea, pDock);
    m_pViewMenu->addAction(pDock->toggleViewAction());

    //setup file differences dock
    pDock = new QDockWidget(tr("Differences"), this);
    DiffView *pDV = new DiffView(pDock);
    pDock->setWidget(pDV);
    m_docks["file_diff"] = pDock;
    pDock->setObjectName("MainWindow/Differences");
    addDockWidget(Qt::BottomDockWidgetArea, pDock);
    m_pViewMenu->addAction(pDock->toggleViewAction());

    //setup staged dock
    pDock = new QDockWidget(tr("Staged"));
    m_docks["staged"] = pDock;
    pDock->setObjectName("MainWindow/Staged");
    addDockWidget(Qt::RightDockWidgetArea, pDock);
    m_pViewMenu->addAction(pDock->toggleViewAction());
    pView = new FileView(pDock);
    pDock->setWidget(pView);
    pView->setModel(new GBL_FileModel(pView));

    //setup unstaged dock
    pDock = new QDockWidget(tr("Unstaged"));
    m_docks["unstaged"] = pDock;
    pDock->setObjectName("MainWindow/Unstaged");
    addDockWidget(Qt::RightDockWidgetArea, pDock);
    m_pViewMenu->addAction(pDock->toggleViewAction());
    pView = new FileView(pDock);
    pDock->setWidget(pView);
    pView->setModel(new GBL_FileModel(pView));
}

void MainWindow::readSettings()
{
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    const QByteArray geometry = settings.value("MainWindow/Geometry", QByteArray()).toByteArray();
    if (geometry.isEmpty()) {
        const QRect availableGeometry = QApplication::desktop()->availableGeometry(this);
        resize(availableGeometry.width() / 3, availableGeometry.height() / 2);
        move((availableGeometry.width() - width()) / 2,
             (availableGeometry.height() - height()) / 2);
    } else {
        restoreGeometry(geometry);
    }

    const QByteArray state = settings.value("MainWindow/WindowState", QByteArray()).toByteArray();
    if (!state.isEmpty())
    {
        restoreState(state);
    }

    m_pNetAM = new QNetworkAccessManager(this);

    //create cache dir
    QString sCachePath = GBL_Storage::getCachePath();
    QDir cachePath(sCachePath);
    if (!cachePath.exists())
    {
        cachePath.mkpath(sCachePath);
    }

    m_pNetCache = new QNetworkDiskCache(this);
    m_pNetCache->setCacheDirectory(sCachePath);
    m_pNetAM->setCache(m_pNetCache);
}

void MainWindow::writeSettings()
{
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    settings.setValue("MainWindow/Geometry", saveGeometry());
    settings.setValue("MainWindow/WindowState", saveState());
}

static inline QString RecentReposKey() { return QStringLiteral("RecentRepoList"); }
static inline QString fileKey() { return QStringLiteral("file"); }

static QStringList readRecentRepos(QSettings &settings)
{
    QStringList result;
    const int count = settings.beginReadArray(RecentReposKey());
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        result.append(settings.value(fileKey()).toString());
    }
    settings.endArray();
    return result;
}

static void writeRecentRepos(const QStringList &files, QSettings &settings)
{
    const int count = files.size();
    settings.beginWriteArray(RecentReposKey());
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        settings.setValue(fileKey(), files.at(i));
    }
    settings.endArray();
}

bool MainWindow::hasRecentRepos()
{
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    const int count = settings.beginReadArray(RecentReposKey());
    settings.endArray();
    return count > 0;
}

void MainWindow::prependToRecentRepos(const QString &dirName)
{
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());

    const QStringList oldRecentRepos = readRecentRepos(settings);
    QStringList RecentRepos = oldRecentRepos;
    RecentRepos.removeAll(dirName);
    RecentRepos.prepend(dirName);
    if (oldRecentRepos != RecentRepos)
        writeRecentRepos(RecentRepos, settings);

    setRecentReposVisible(!RecentRepos.isEmpty());
}

void MainWindow::setRecentReposVisible(bool visible)
{
    m_pRecentRepoSubMenuAct->setVisible(visible);
    m_pRecentRepoSeparator->setVisible(visible);
}

void MainWindow::updateRecentRepoActions()
{
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());

    const QStringList RecentRepos = readRecentRepos(settings);
    const int count = qMin(int(MaxRecentRepos), RecentRepos.size());
    int i = 0;
    for ( ; i < count; ++i) {
        const QString fileName = QFileInfo(RecentRepos.at(i)).fileName();
        m_pRecentRepoActs[i]->setText(tr("&%1 %2").arg(i + 1).arg(fileName));
        m_pRecentRepoActs[i]->setData(RecentRepos.at(i));
        m_pRecentRepoActs[i]->setVisible(true);
    }
    for ( ; i < MaxRecentRepos; ++i)
        m_pRecentRepoActs[i]->setVisible(false);
}

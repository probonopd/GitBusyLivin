#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPointer>
#include <QMap>


QT_BEGIN_NAMESPACE
class QAction;
class QMenu;
class QPlainTextEdit;
class QSessionManager;
class HistoryView;
class GBL_HistoryModel;
class GBL_Repository;
class QDockWidget;
class QItemSelection;
class QNetworkAccessManager;
class QNetworkDiskCache;
class QAction;
struct GBL_Line_Item;
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = 0);
    ~MainWindow();

    QNetworkAccessManager* getNetworkAccessManager() { return m_pNetAM; }
    QNetworkDiskCache* getNetworkCache() { return m_pNetCache; }

    void addToDiffView(GBL_Line_Item *pLineItem);
    void setTheme(const QString &theme);
    QString getTheme() { return m_sTheme; }

    GBL_Repository* getRepo() { return m_qpRepo; }
    QToolBar* getToolBar() { return m_pToolBar; }
    static MainWindow* getInstance() { return m_pSingleInst; }
    static void setInstance(MainWindow* pInst) { m_pSingleInst = pInst; }

public slots:
    void stageAll();
    void stageSelected();
    void unstageAll();
    void unstageSelected();
    void commit();

private slots:
    void about();
    void clone();
    void open();
    void new_local_repo();
    void new_network_repo();
    void preferences();
    void toggleToolBar();
    void toggleStatusBar();
    void sslVersion();
    void historySelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
    void historyFileSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
    void workingFileSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
    void stagedFileSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
    void closeEvent(QCloseEvent *event);
    void updateRecentRepoActions();
    void openRecentRepo();
    void timerEvent(QTimerEvent *event);
    void refresh();


private:
    enum { MaxRecentRepos = 10 };

    void init();
    void cleanupDocks();
    void createActions();
    void createDocks();
    void createHistoryTable();
    void readSettings();
    void writeSettings();
    void setupRepoUI(QString repoDir);
    static bool hasRecentRepos();
    void prependToRecentRepos(const QString &dirName);
    void setRecentReposVisible(bool visible);
    void updateStatus();

    GBL_Repository *m_qpRepo;
    QString m_sRepoPath;
    HistoryView *m_pHistView;
    QPointer<GBL_HistoryModel> m_pHistModel;
    QMap<QString, QDockWidget*> m_docks;
    QMenu *m_pViewMenu, *m_pRepoMenu;
    QToolBar *m_pToolBar;
    QNetworkAccessManager *m_pNetAM;
    QNetworkDiskCache *m_pNetCache;
    QString m_sTheme;

    QAction *m_pRecentRepoActs[MaxRecentRepos];
    QAction *m_pRecentRepoSeparator;
    QAction *m_pRecentRepoSubMenuAct;
    QMap<QString, QAction*> m_actionMap;

    static MainWindow *m_pSingleInst;
    int m_updateTimer;

};

#endif // MAINWINDOW_H

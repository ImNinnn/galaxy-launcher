#pragma once

#include <QMainWindow>
#include <QSortFilterProxyModel>

#include "Application.h"
#include "minecraft/MinecraftInstance.h"
#include "ui/pages/BasePage.h"

class ResourceFolderModel;

namespace Ui {
class ExternalResourcesPage;
}

/* This page is used as a base for pages in which the user can manage external resources
 * related to the game, such as mods, shaders or resource packs. */
class ExternalResourcesPage : public QMainWindow, public BasePage {
    Q_OBJECT

   public:
    explicit ExternalResourcesPage(MinecraftInstance* instance, ResourceFolderModel* model, QWidget* parent = nullptr);
    ~ExternalResourcesPage() override;

    QString displayName() const override = 0;
    QIcon icon() const override = 0;
    QString id() const override = 0;
    QString helpPage() const override = 0;

    bool shouldDisplay() const override = 0;
    QString extraHeaderInfoString();

    void openedImpl() override;
    void closedImpl() override;

    void retranslate() override;

   protected:
    bool event(QEvent* ev) override;
    bool eventFilter(QObject* obj, QEvent* ev) override;
    bool listFilter(QKeyEvent* keyEvent);
    QMenu* createPopupMenu() override;

   public slots:
    virtual void updateActions();
    virtual void updateFrame(const QModelIndex& current, const QModelIndex& previous);

   protected slots:
    void itemActivated(const QModelIndex& index);
    void filterTextChanged(const QString& newContents);

    virtual void addItem();
    void removeItem();
    virtual void removeItems(const QItemSelection& selection);

    virtual void enableItem();
    virtual void disableItem();

    virtual void viewHomepage();

    virtual void viewFolder();
    virtual void viewConfigs();

    void showContextMenu(const QPoint& pos);
    void showHeaderContextMenu(const QPoint& pos);

    void lockUpdates();
    void unlockUpdates();

   protected:
    /**
     * Galaxy Launcher: trims the toolbar to its own set (no update checks or update locks; updates are shown in the
     * galaxy and offered per project) and renames "Download" and "View Homepage". Called at the end of each page's
     * constructor, once its actions are in place.
     */
    void applyGalaxyActions();
    /** Galaxy Launcher: enables "Update" and shows "Update All" when the profile has updates. */
    void updateGalaxyActions();
    QAction* m_actionUpdate = nullptr;     ///< Updates the selected project to its latest version.
    QAction* m_actionUpdateAll = nullptr;  ///< Every update in the profile (UpdatesDialog).
    QAction* m_updateAllBarEntry = nullptr;  ///< Its button's place in the toolbar (hiding the action does not hide it).

    MinecraftInstance* m_instance = nullptr;

    Ui::ExternalResourcesPage* m_ui = nullptr;
    ResourceFolderModel* m_model;
    QSortFilterProxyModel* m_filterModel = nullptr;

    QString m_fileSelectionFilter;
    QString m_viewFilter;
};

#include <QtWidgets>
#include <QList>
#include <QPrinter>
#include <QPrintDialog>
#include <QPainter>
#include "vmainwindow.h"
#include "vdirectorytree.h"
#include "vnote.h"
#include "vfilelist.h"
#include "vconfigmanager.h"
#include "utils/vutils.h"
#include "veditarea.h"
#include "voutline.h"
#include "vnotebookselector.h"
#include "vavatar.h"
#include "dialog/vfindreplacedialog.h"
#include "dialog/vsettingsdialog.h"
#include "vcaptain.h"
#include "vedittab.h"
#include "vwebview.h"
#include "vexporter.h"
#include "vmdtab.h"
#include "vvimindicator.h"
#include "vtabindicator.h"
#include "dialog/vupdater.h"

extern VConfigManager vconfig;

VNote *g_vnote;

#if defined(QT_NO_DEBUG)
extern QFile g_logFile;
#endif

VMainWindow::VMainWindow(QWidget *parent)
    : QMainWindow(parent), m_onePanel(false)
{
    setWindowIcon(QIcon(":/resources/icons/vnote.ico"));
    vnote = new VNote(this);
    g_vnote = vnote;
    vnote->initPalette(palette());
    initPredefinedColorPixmaps();

    setupUI();

    initMenuBar();
    initToolBar();
    initDockWindows();
    initAvatar();

    restoreStateAndGeometry();
    setContextMenuPolicy(Qt::NoContextMenu);

    notebookSelector->update();

    initCaptain();
}

void VMainWindow::initCaptain()
{
    // VCaptain should be visible to accpet key focus. But VCaptain
    // may hide other widgets.
    m_captain = new VCaptain(this);
    connect(m_captain, &VCaptain::captainModeChanged,
            this, &VMainWindow::handleCaptainModeChanged);

    m_captain->registerNavigationTarget(notebookSelector);
    m_captain->registerNavigationTarget(directoryTree);
    m_captain->registerNavigationTarget(fileList);
    m_captain->registerNavigationTarget(editArea);
    m_captain->registerNavigationTarget(outline);
}

void VMainWindow::setupUI()
{
    QWidget *directoryPanel = setupDirectoryPanel();

    fileList = new VFileList();
    fileList->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Expanding);

    editArea = new VEditArea(vnote);
    editArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_findReplaceDialog = editArea->getFindReplaceDialog();
    fileList->setEditArea(editArea);
    directoryTree->setEditArea(editArea);
    notebookSelector->setEditArea(editArea);

    // Main Splitter
    mainSplitter = new QSplitter();
    mainSplitter->setObjectName("MainSplitter");
    mainSplitter->addWidget(directoryPanel);
    mainSplitter->addWidget(fileList);
    mainSplitter->addWidget(editArea);
    mainSplitter->setStretchFactor(0, 0);
    mainSplitter->setStretchFactor(1, 0);
    mainSplitter->setStretchFactor(2, 1);

    // Signals
    connect(directoryTree, &VDirectoryTree::currentDirectoryChanged,
            fileList, &VFileList::setDirectory);
    connect(directoryTree, &VDirectoryTree::directoryUpdated,
            editArea, &VEditArea::handleDirectoryUpdated);

    connect(notebookSelector, &VNotebookSelector::notebookUpdated,
            editArea, &VEditArea::handleNotebookUpdated);

    connect(fileList, &VFileList::fileClicked,
            editArea, &VEditArea::openFile);
    connect(fileList, &VFileList::fileCreated,
            editArea, &VEditArea::openFile);
    connect(fileList, &VFileList::fileUpdated,
            editArea, &VEditArea::handleFileUpdated);
    connect(editArea, &VEditArea::tabStatusUpdated,
            this, &VMainWindow::handleAreaTabStatusUpdated);
    connect(editArea, &VEditArea::statusMessage,
            this, &VMainWindow::showStatusMessage);
    connect(editArea, &VEditArea::vimStatusUpdated,
            this, &VMainWindow::handleVimStatusUpdated);
    connect(m_findReplaceDialog, &VFindReplaceDialog::findTextChanged,
            this, &VMainWindow::handleFindDialogTextChanged);

    setCentralWidget(mainSplitter);

    m_vimIndicator = new VVimIndicator(this);
    m_vimIndicator->hide();

    m_tabIndicator = new VTabIndicator(this);
    m_tabIndicator->hide();

    // Create and show the status bar
    statusBar()->addPermanentWidget(m_vimIndicator);
    statusBar()->addPermanentWidget(m_tabIndicator);
}

QWidget *VMainWindow::setupDirectoryPanel()
{
    notebookLabel = new QLabel(tr("Notebooks"));
    notebookLabel->setProperty("TitleLabel", true);
    notebookLabel->setProperty("NotebookPanel", true);
    directoryLabel = new QLabel(tr("Folders"));
    directoryLabel->setProperty("TitleLabel", true);
    directoryLabel->setProperty("NotebookPanel", true);

    notebookSelector = new VNotebookSelector(vnote);
    notebookSelector->setObjectName("NotebookSelector");
    notebookSelector->setProperty("NotebookPanel", true);
    notebookSelector->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);

    directoryTree = new VDirectoryTree(vnote);
    directoryTree->setProperty("NotebookPanel", true);

    QVBoxLayout *nbLayout = new QVBoxLayout;
    nbLayout->addWidget(notebookLabel);
    nbLayout->addWidget(notebookSelector);
    nbLayout->addWidget(directoryLabel);
    nbLayout->addWidget(directoryTree);
    nbLayout->setContentsMargins(0, 0, 0, 0);
    nbLayout->setSpacing(0);
    QWidget *nbContainer = new QWidget();
    nbContainer->setObjectName("NotebookPanel");
    nbContainer->setLayout(nbLayout);
    nbContainer->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Expanding);

    connect(notebookSelector, &VNotebookSelector::curNotebookChanged,
            directoryTree, &VDirectoryTree::setNotebook);
    connect(notebookSelector, &VNotebookSelector::curNotebookChanged,
            this, &VMainWindow::handleCurrentNotebookChanged);

    connect(directoryTree, &VDirectoryTree::currentDirectoryChanged,
            this, &VMainWindow::handleCurrentDirectoryChanged);
    return nbContainer;
}

void VMainWindow::initToolBar()
{
    initFileToolBar();
    initViewToolBar();
    initEditToolBar();
}

void VMainWindow::initViewToolBar()
{
    QToolBar *viewToolBar = addToolBar(tr("View"));
    viewToolBar->setObjectName("ViewToolBar");
    viewToolBar->setMovable(false);

    QAction *onePanelViewAct = new QAction(QIcon(":/resources/icons/one_panel.svg"),
                                           tr("&Single Panel"), this);
    onePanelViewAct->setStatusTip(tr("Display only the notes list panel"));
    connect(onePanelViewAct, &QAction::triggered,
            this, &VMainWindow::onePanelView);

    QAction *twoPanelViewAct = new QAction(QIcon(":/resources/icons/two_panels.svg"),
                                           tr("&Two Panels"), this);
    twoPanelViewAct->setStatusTip(tr("Display both the folders and notes list panel"));
    connect(twoPanelViewAct, &QAction::triggered,
            this, &VMainWindow::twoPanelView);

    QMenu *panelMenu = new QMenu(this);
    panelMenu->addAction(onePanelViewAct);
    panelMenu->addAction(twoPanelViewAct);

    expandViewAct = new QAction(QIcon(":/resources/icons/expand.svg"),
                                tr("Expand"), this);
    expandViewAct->setStatusTip(tr("Expand the edit area"));
    expandViewAct->setCheckable(true);
    expandViewAct->setMenu(panelMenu);
    connect(expandViewAct, &QAction::triggered,
            this, &VMainWindow::expandPanelView);

    viewToolBar->addAction(expandViewAct);
}

static void setActionsVisible(QWidget *p_widget, bool p_visible)
{
    Q_ASSERT(p_widget);
    QList<QAction *> actions = p_widget->actions();
    for (auto const & act : actions) {
        act->setVisible(p_visible);
    }
}

void VMainWindow::initEditToolBar()
{
    m_editToolBar = addToolBar(tr("Edit Toolbar"));
    m_editToolBar->setObjectName("EditToolBar");
    m_editToolBar->setMovable(false);

    m_editToolBar->addSeparator();

    QAction *boldAct = new QAction(QIcon(":/resources/icons/bold.svg"),
                                   tr("Bold"), this);
    boldAct->setStatusTip(tr("Insert bold text or change selected text to bold"));
    connect(boldAct, &QAction::triggered,
            this, [this](){
                if (m_curTab) {
                    m_curTab->decorateText(TextDecoration::Bold);
                }
            });

    m_editToolBar->addAction(boldAct);

    QAction *italicAct = new QAction(QIcon(":/resources/icons/italic.svg"),
                                     tr("Italic"), this);
    italicAct->setStatusTip(tr("Insert italic text or change selected text to italic"));
    connect(italicAct, &QAction::triggered,
            this, [this](){
                if (m_curTab) {
                    m_curTab->decorateText(TextDecoration::Italic);
                }
            });

    m_editToolBar->addAction(italicAct);

    QAction *underlineAct = new QAction(QIcon(":/resources/icons/underline.svg"),
                                        tr("Underline"), this);
    underlineAct->setStatusTip(tr("Insert underlined text or change selected text to underlined"));
    connect(underlineAct, &QAction::triggered,
            this, [this](){
                if (m_curTab) {
                    m_curTab->decorateText(TextDecoration::Underline);
                }
            });

    m_editToolBar->addAction(underlineAct);

    QAction *strikethroughAct = new QAction(QIcon(":/resources/icons/strikethrough.svg"),
                                            tr("Strikethrough"), this);
    strikethroughAct->setStatusTip(tr("Insert strikethrough text or change selected text to strikethroughed"));
    connect(strikethroughAct, &QAction::triggered,
            this, [this](){
                if (m_curTab) {
                    m_curTab->decorateText(TextDecoration::Strikethrough);
                }
            });

    m_editToolBar->addAction(strikethroughAct);

    QAction *inlineCodeAct = new QAction(QIcon(":/resources/icons/inline_code.svg"),
                                         tr("Inline Code"), this);
    inlineCodeAct->setStatusTip(tr("Insert inline-code text or change selected text to inline-coded"));
    connect(inlineCodeAct, &QAction::triggered,
            this, [this](){
                if (m_curTab) {
                    m_curTab->decorateText(TextDecoration::InlineCode);
                }
            });

    m_editToolBar->addAction(inlineCodeAct);

    QAction *toggleAct = m_editToolBar->toggleViewAction();
    toggleAct->setToolTip(tr("Toggle the edit toolbar"));
    viewMenu->addAction(toggleAct);

    setActionsVisible(m_editToolBar, false);
}

void VMainWindow::initFileToolBar()
{
    QToolBar *fileToolBar = addToolBar(tr("Note"));
    fileToolBar->setObjectName("NoteToolBar");
    fileToolBar->setMovable(false);

    newRootDirAct = new QAction(QIcon(":/resources/icons/create_rootdir_tb.svg"),
                                tr("New &Root Folder"), this);
    newRootDirAct->setStatusTip(tr("Create a root folder in current notebook"));
    connect(newRootDirAct, &QAction::triggered,
            directoryTree, &VDirectoryTree::newRootDirectory);

    newNoteAct = new QAction(QIcon(":/resources/icons/create_note_tb.svg"),
                             tr("New &Note"), this);
    newNoteAct->setStatusTip(tr("Create a note in current folder"));
    newNoteAct->setShortcut(QKeySequence::New);
    connect(newNoteAct, &QAction::triggered,
            fileList, &VFileList::newFile);

    noteInfoAct = new QAction(QIcon(":/resources/icons/note_info_tb.svg"),
                              tr("Note &Info"), this);
    noteInfoAct->setStatusTip(tr("View and edit current note's information"));
    connect(noteInfoAct, &QAction::triggered,
            this, &VMainWindow::curEditFileInfo);

    deleteNoteAct = new QAction(QIcon(":/resources/icons/delete_note_tb.svg"),
                                tr("&Delete Note"), this);
    deleteNoteAct->setStatusTip(tr("Delete current note"));
    connect(deleteNoteAct, &QAction::triggered,
            this, &VMainWindow::deleteCurNote);

    editNoteAct = new QAction(QIcon(":/resources/icons/edit_note.svg"),
                              tr("&Edit"), this);
    editNoteAct->setStatusTip(tr("Edit current note"));
    editNoteAct->setShortcut(QKeySequence("Ctrl+W"));
    connect(editNoteAct, &QAction::triggered,
            editArea, &VEditArea::editFile);

    discardExitAct = new QAction(QIcon(":/resources/icons/discard_exit.svg"),
                                 tr("Discard Changes And Read"), this);
    discardExitAct->setStatusTip(tr("Discard changes and exit edit mode"));
    connect(discardExitAct, &QAction::triggered,
            editArea, &VEditArea::readFile);

    QMenu *exitEditMenu = new QMenu(this);
    exitEditMenu->addAction(discardExitAct);

    saveExitAct = new QAction(QIcon(":/resources/icons/save_exit.svg"),
                              tr("Save Changes And Read"), this);
    saveExitAct->setStatusTip(tr("Save changes and exit edit mode"));
    saveExitAct->setMenu(exitEditMenu);
    saveExitAct->setShortcut(QKeySequence("Ctrl+T"));
    connect(saveExitAct, &QAction::triggered,
            editArea, &VEditArea::saveAndReadFile);

    saveNoteAct = new QAction(QIcon(":/resources/icons/save_note.svg"),
                              tr("Save"), this);
    saveNoteAct->setStatusTip(tr("Save changes to current note"));
    saveNoteAct->setShortcut(QKeySequence::Save);
    connect(saveNoteAct, &QAction::triggered,
            editArea, &VEditArea::saveFile);

    newRootDirAct->setEnabled(false);
    newNoteAct->setEnabled(false);
    noteInfoAct->setEnabled(false);
    deleteNoteAct->setEnabled(false);
    editNoteAct->setVisible(false);
    saveExitAct->setVisible(false);
    discardExitAct->setVisible(false);
    saveNoteAct->setVisible(false);

    fileToolBar->addAction(newRootDirAct);
    fileToolBar->addAction(newNoteAct);
    fileToolBar->addAction(noteInfoAct);
    fileToolBar->addAction(deleteNoteAct);
    fileToolBar->addSeparator();
    fileToolBar->addAction(editNoteAct);
    fileToolBar->addAction(saveExitAct);
    fileToolBar->addAction(saveNoteAct);
    fileToolBar->addSeparator();
}

void VMainWindow::initMenuBar()
{
    initFileMenu();
    initEditMenu();
    initViewMenu();
    initMarkdownMenu();
    initHelpMenu();
}

void VMainWindow::initHelpMenu()
{
    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->setToolTipsVisible(true);

#if defined(QT_NO_DEBUG)
    QAction *logAct = new QAction(tr("View &Log"), this);
    logAct->setToolTip(tr("View VNote's debug log (%1)").arg(vconfig.getLogFilePath()));
    connect(logAct, &QAction::triggered,
            this, [](){
                QUrl url = QUrl::fromLocalFile(vconfig.getLogFilePath());
                QDesktopServices::openUrl(url);
            });
#endif

    QAction *shortcutAct = new QAction(tr("&Shortcuts Help"), this);
    shortcutAct->setToolTip(tr("View information about shortcut keys"));
    connect(shortcutAct, &QAction::triggered,
            this, &VMainWindow::shortcutHelp);

    QAction *updateAct = new QAction(tr("Check For &Updates"), this);
    updateAct->setToolTip(tr("Check for updates of VNote"));
    connect(updateAct, &QAction::triggered,
            this, [this]() {
                VUpdater updater(this);
                updater.exec();
            });

    QAction *starAct = new QAction(tr("Star VNote on &Github"), this);
    starAct->setToolTip(tr("Give a star to VNote on Github project"));
    connect(starAct, &QAction::triggered,
            this, [this]() {
                QString url("https://github.com/tamlok/vnote");
                QDesktopServices::openUrl(url);
            });

    QAction *feedbackAct = new QAction(tr("&Feedback"), this);
    feedbackAct->setToolTip(tr("Open an issue on Github"));
    connect(feedbackAct, &QAction::triggered,
            this, [this]() {
                QString url("https://github.com/tamlok/vnote/issues");
                QDesktopServices::openUrl(url);
            });

    QAction *aboutAct = new QAction(tr("&About VNote"), this);
    aboutAct->setToolTip(tr("View information about VNote"));
    aboutAct->setMenuRole(QAction::AboutRole);
    connect(aboutAct, &QAction::triggered,
            this, &VMainWindow::aboutMessage);

    QAction *aboutQtAct = new QAction(tr("About &Qt"), this);
    aboutQtAct->setToolTip(tr("View information about Qt"));
    aboutQtAct->setMenuRole(QAction::AboutQtRole);
    connect(aboutQtAct, &QAction::triggered,
            qApp, &QApplication::aboutQt);

#if defined(QT_NO_DEBUG)
    helpMenu->addAction(logAct);
#endif

    helpMenu->addAction(shortcutAct);
    helpMenu->addAction(updateAct);
    helpMenu->addAction(starAct);
    helpMenu->addAction(feedbackAct);
    helpMenu->addAction(aboutQtAct);
    helpMenu->addAction(aboutAct);
}

void VMainWindow::initMarkdownMenu()
{
    QMenu *markdownMenu = menuBar()->addMenu(tr("&Markdown"));
    markdownMenu->setToolTipsVisible(true);
    QMenu *converterMenu = markdownMenu->addMenu(tr("&Converter"));
    converterMenu->setToolTipsVisible(true);

    QActionGroup *converterAct = new QActionGroup(this);
    QAction *markedAct = new QAction(tr("Marked"), converterAct);
    markedAct->setToolTip(tr("Use Marked to convert Markdown to HTML (re-open current tabs to make it work)"));
    markedAct->setCheckable(true);
    markedAct->setData(int(MarkdownConverterType::Marked));

    QAction *hoedownAct = new QAction(tr("Hoedown"), converterAct);
    hoedownAct->setToolTip(tr("Use Hoedown to convert Markdown to HTML (re-open current tabs to make it work)"));
    hoedownAct->setCheckable(true);
    hoedownAct->setData(int(MarkdownConverterType::Hoedown));

    QAction *markdownitAct = new QAction(tr("Markdown-it"), converterAct);
    markdownitAct->setToolTip(tr("Use Markdown-it to convert Markdown to HTML (re-open current tabs to make it work)"));
    markdownitAct->setCheckable(true);
    markdownitAct->setData(int(MarkdownConverterType::MarkdownIt));

    QAction *showdownAct = new QAction(tr("Showdown"), converterAct);
    showdownAct->setToolTip(tr("Use Showdown to convert Markdown to HTML (re-open current tabs to make it work)"));
    showdownAct->setCheckable(true);
    showdownAct->setData(int(MarkdownConverterType::Showdown));

    connect(converterAct, &QActionGroup::triggered,
            this, &VMainWindow::changeMarkdownConverter);
    converterMenu->addAction(hoedownAct);
    converterMenu->addAction(markedAct);
    converterMenu->addAction(markdownitAct);
    converterMenu->addAction(showdownAct);

    MarkdownConverterType converterType = vconfig.getMdConverterType();
    switch (converterType) {
    case MarkdownConverterType::Marked:
        markedAct->setChecked(true);
        break;

    case MarkdownConverterType::Hoedown:
        hoedownAct->setChecked(true);
        break;

    case MarkdownConverterType::MarkdownIt:
        markdownitAct->setChecked(true);
        break;

    case MarkdownConverterType::Showdown:
        showdownAct->setChecked(true);
        break;

    default:
        Q_ASSERT(false);
    }

    initRenderStyleMenu(markdownMenu);

    initRenderBackgroundMenu(markdownMenu);

    QAction *constrainImageAct = new QAction(tr("Constrain The Width of Images"), this);
    constrainImageAct->setToolTip(tr("Constrain the width of images to the window in read mode (re-open current tabs to make it work)"));
    constrainImageAct->setCheckable(true);
    connect(constrainImageAct, &QAction::triggered,
            this, &VMainWindow::enableImageConstraint);
    markdownMenu->addAction(constrainImageAct);
    constrainImageAct->setChecked(vconfig.getEnableImageConstraint());

    QAction *imageCaptionAct = new QAction(tr("Enable Image Caption"), this);
    imageCaptionAct->setToolTip(tr("Center the images and display the alt text as caption (re-open current tabs to make it work)"));
    imageCaptionAct->setCheckable(true);
    connect(imageCaptionAct, &QAction::triggered,
            this, &VMainWindow::enableImageCaption);
    markdownMenu->addAction(imageCaptionAct);
    imageCaptionAct->setChecked(vconfig.getEnableImageCaption());

    markdownMenu->addSeparator();

    QAction *mermaidAct = new QAction(tr("&Mermaid Diagram"), this);
    mermaidAct->setToolTip(tr("Enable Mermaid for graph and diagram"));
    mermaidAct->setCheckable(true);
    connect(mermaidAct, &QAction::triggered,
            this, &VMainWindow::enableMermaid);
    markdownMenu->addAction(mermaidAct);

    mermaidAct->setChecked(vconfig.getEnableMermaid());

    QAction *flowchartAct = new QAction(tr("&Flowchart.js"), this);
    flowchartAct->setToolTip(tr("Enable Flowchart.js for flowchart diagram"));
    flowchartAct->setCheckable(true);
    connect(flowchartAct, &QAction::triggered,
            this, [this](bool p_enabled){
                vconfig.setEnableFlowchart(p_enabled);
            });
    markdownMenu->addAction(flowchartAct);
    flowchartAct->setChecked(vconfig.getEnableFlowchart());

    QAction *mathjaxAct = new QAction(tr("Math&Jax"), this);
    mathjaxAct->setToolTip(tr("Enable MathJax for math support in Markdown"));
    mathjaxAct->setCheckable(true);
    connect(mathjaxAct, &QAction::triggered,
            this, &VMainWindow::enableMathjax);
    markdownMenu->addAction(mathjaxAct);

    mathjaxAct->setChecked(vconfig.getEnableMathjax());

    markdownMenu->addSeparator();

    QAction *codeBlockAct = new QAction(tr("Highlight Code Blocks In Edit Mode"), this);
    codeBlockAct->setToolTip(tr("Enable syntax highlight within code blocks in edit mode"));
    codeBlockAct->setCheckable(true);
    connect(codeBlockAct, &QAction::triggered,
            this, &VMainWindow::enableCodeBlockHighlight);
    markdownMenu->addAction(codeBlockAct);
    codeBlockAct->setChecked(vconfig.getEnableCodeBlockHighlight());

    QAction *previewImageAct = new QAction(tr("Preview Images In Edit Mode"), this);
    previewImageAct->setToolTip(tr("Enable image preview in edit mode"));
    previewImageAct->setCheckable(true);
    connect(previewImageAct, &QAction::triggered,
            this, &VMainWindow::enableImagePreview);
    // TODO: add the action to the menu after handling the UNDO history well.
    // markdownMenu->addAction(previewImageAct);
    previewImageAct->setChecked(vconfig.getEnablePreviewImages());

    QAction *previewWidthAct = new QAction(tr("Constrain The Width Of Previewed Images"), this);
    previewWidthAct->setToolTip(tr("Constrain the width of previewed images to the edit window in edit mode"));
    previewWidthAct->setCheckable(true);
    connect(previewWidthAct, &QAction::triggered,
            this, &VMainWindow::enableImagePreviewConstraint);
    markdownMenu->addAction(previewWidthAct);
    previewWidthAct->setChecked(vconfig.getEnablePreviewImageConstraint());
}

void VMainWindow::initViewMenu()
{
    viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->setToolTipsVisible(true);
}

void VMainWindow::initFileMenu()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->setToolTipsVisible(true);

    // Import notes from files.
    m_importNoteAct = newAction(QIcon(":/resources/icons/import_note.svg"),
                                tr("&Import Notes From Files"), this);
    m_importNoteAct->setToolTip(tr("Import notes from external files into current folder"));
    connect(m_importNoteAct, &QAction::triggered,
            this, &VMainWindow::importNoteFromFile);
    m_importNoteAct->setEnabled(false);

    fileMenu->addAction(m_importNoteAct);

    fileMenu->addSeparator();

    // Export as PDF.
    m_exportAsPDFAct = new QAction(tr("Export As &PDF"), this);
    m_exportAsPDFAct->setToolTip(tr("Export current note as PDF file"));
    connect(m_exportAsPDFAct, &QAction::triggered,
            this, &VMainWindow::exportAsPDF);
    m_exportAsPDFAct->setEnabled(false);

    fileMenu->addAction(m_exportAsPDFAct);

    fileMenu->addSeparator();

    // Print.
    m_printAct = new QAction(QIcon(":/resources/icons/print.svg"),
                             tr("&Print"), this);
    m_printAct->setToolTip(tr("Print current note"));
    connect(m_printAct, &QAction::triggered,
            this, &VMainWindow::printNote);
    m_printAct->setEnabled(false);

    // Settings.
    QAction *settingsAct = newAction(QIcon(":/resources/icons/settings.svg"),
                                     tr("&Settings"), this);
    settingsAct->setToolTip(tr("View and change settings for VNote"));
    settingsAct->setMenuRole(QAction::PreferencesRole);
    connect(settingsAct, &QAction::triggered,
            this, &VMainWindow::viewSettings);

    fileMenu->addAction(settingsAct);

    fileMenu->addSeparator();

    // Exit.
    QAction *exitAct = new QAction(tr("&Exit"), this);
    exitAct->setToolTip(tr("Exit VNote"));
    exitAct->setShortcut(QKeySequence("Ctrl+Q"));
    exitAct->setMenuRole(QAction::QuitRole);
    connect(exitAct, &QAction::triggered,
            this, &VMainWindow::close);

    fileMenu->addAction(exitAct);
}

void VMainWindow::initEditMenu()
{
    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->setToolTipsVisible(true);

    // Insert image.
    m_insertImageAct = newAction(QIcon(":/resources/icons/insert_image.svg"),
                                 tr("Insert &Image"), this);
    m_insertImageAct->setToolTip(tr("Insert an image from file into current note"));
    connect(m_insertImageAct, &QAction::triggered,
            this, &VMainWindow::insertImage);

    // Find/Replace.
    m_findReplaceAct = newAction(QIcon(":/resources/icons/find_replace.svg"),
                                 tr("Find/Replace"), this);
    m_findReplaceAct->setToolTip(tr("Open Find/Replace dialog to search in current note"));
    m_findReplaceAct->setShortcut(QKeySequence::Find);
    connect(m_findReplaceAct, &QAction::triggered,
            this, &VMainWindow::openFindDialog);

    m_findNextAct = new QAction(tr("Find Next"), this);
    m_findNextAct->setToolTip(tr("Find next occurence"));
    m_findNextAct->setShortcut(QKeySequence::FindNext);
    connect(m_findNextAct, SIGNAL(triggered(bool)),
            m_findReplaceDialog, SLOT(findNext()));

    m_findPreviousAct = new QAction(tr("Find Previous"), this);
    m_findPreviousAct->setToolTip(tr("Find previous occurence"));
    m_findPreviousAct->setShortcut(QKeySequence::FindPrevious);
    connect(m_findPreviousAct, SIGNAL(triggered(bool)),
            m_findReplaceDialog, SLOT(findPrevious()));

    m_replaceAct = new QAction(tr("Replace"), this);
    m_replaceAct->setToolTip(tr("Replace current occurence"));
    connect(m_replaceAct, SIGNAL(triggered(bool)),
            m_findReplaceDialog, SLOT(replace()));

    m_replaceFindAct = new QAction(tr("Replace && Find"), this);
    m_replaceFindAct->setToolTip(tr("Replace current occurence and find the next one"));
    connect(m_replaceFindAct, SIGNAL(triggered(bool)),
            m_findReplaceDialog, SLOT(replaceFind()));

    m_replaceAllAct = new QAction(tr("Replace All"), this);
    m_replaceAllAct->setToolTip(tr("Replace all occurences in current note"));
    connect(m_replaceAllAct, SIGNAL(triggered(bool)),
            m_findReplaceDialog, SLOT(replaceAll()));

    QAction *searchedWordAct = new QAction(tr("Highlight Searched Pattern"), this);
    searchedWordAct->setToolTip(tr("Highlight all occurences of searched pattern"));
    searchedWordAct->setCheckable(true);
    connect(searchedWordAct, &QAction::triggered,
            this, &VMainWindow::changeHighlightSearchedWord);

    // Expand Tab into spaces.
    QAction *expandTabAct = new QAction(tr("&Expand Tab"), this);
    expandTabAct->setToolTip(tr("Expand entered Tab to spaces"));
    expandTabAct->setCheckable(true);
    connect(expandTabAct, &QAction::triggered,
            this, &VMainWindow::changeExpandTab);

    // Tab stop width.
    QActionGroup *tabStopWidthAct = new QActionGroup(this);
    QAction *twoSpaceTabAct = new QAction(tr("2 Spaces"), tabStopWidthAct);
    twoSpaceTabAct->setToolTip(tr("Expand Tab to 2 spaces"));
    twoSpaceTabAct->setCheckable(true);
    twoSpaceTabAct->setData(2);
    QAction *fourSpaceTabAct = new QAction(tr("4 Spaces"), tabStopWidthAct);
    fourSpaceTabAct->setToolTip(tr("Expand Tab to 4 spaces"));
    fourSpaceTabAct->setCheckable(true);
    fourSpaceTabAct->setData(4);
    QAction *eightSpaceTabAct = new QAction(tr("8 Spaces"), tabStopWidthAct);
    eightSpaceTabAct->setToolTip(tr("Expand Tab to 8 spaces"));
    eightSpaceTabAct->setCheckable(true);
    eightSpaceTabAct->setData(8);
    connect(tabStopWidthAct, &QActionGroup::triggered,
            this, &VMainWindow::setTabStopWidth);

    // Auto Indent.
    m_autoIndentAct = new QAction(tr("Auto Indent"), this);
    m_autoIndentAct->setToolTip(tr("Indent automatically when inserting a new line"));
    m_autoIndentAct->setCheckable(true);
    connect(m_autoIndentAct, &QAction::triggered,
            this, &VMainWindow::changeAutoIndent);

    // Auto List.
    QAction *autoListAct = new QAction(tr("Auto List"), this);
    autoListAct->setToolTip(tr("Continue the list automatically when inserting a new line"));
    autoListAct->setCheckable(true);
    connect(autoListAct, &QAction::triggered,
            this, &VMainWindow::changeAutoList);

    // Vim Mode.
    QAction *vimAct = new QAction(tr("Vim Mode"), this);
    vimAct->setToolTip(tr("Enable Vim mode for editing (re-open current tabs to make it work)"));
    vimAct->setCheckable(true);
    connect(vimAct, &QAction::triggered,
            this, &VMainWindow::changeVimMode);

    // Smart input method in Vim mode.
    QAction *smartImAct = new QAction(tr("Smart Input Method In Vim Mode"), this);
    smartImAct->setToolTip(tr("Disable input method when leaving Insert mode in Vim mode"));
    smartImAct->setCheckable(true);
    connect(smartImAct, &QAction::triggered,
            this, [this](bool p_checked){
                vconfig.setEnableSmartImInVimMode(p_checked);
            });

    // Highlight current cursor line.
    QAction *cursorLineAct = new QAction(tr("Highlight Cursor Line"), this);
    cursorLineAct->setToolTip(tr("Highlight current cursor line"));
    cursorLineAct->setCheckable(true);
    connect(cursorLineAct, &QAction::triggered,
            this, &VMainWindow::changeHighlightCursorLine);

    // Highlight selected word.
    QAction *selectedWordAct = new QAction(tr("Highlight Selected Words"), this);
    selectedWordAct->setToolTip(tr("Highlight all occurences of selected words"));
    selectedWordAct->setCheckable(true);
    connect(selectedWordAct, &QAction::triggered,
            this, &VMainWindow::changeHighlightSelectedWord);

    // Highlight trailing space.
    QAction *trailingSapceAct = new QAction(tr("Highlight Trailing Sapces"), this);
    trailingSapceAct->setToolTip(tr("Highlight all the spaces at the end of a line"));
    trailingSapceAct->setCheckable(true);
    connect(trailingSapceAct, &QAction::triggered,
            this, &VMainWindow::changeHighlightTrailingSapce);

    editMenu->addAction(m_insertImageAct);
    editMenu->addSeparator();
    m_insertImageAct->setEnabled(false);

    QMenu *findReplaceMenu = editMenu->addMenu(tr("Find/Replace"));
    findReplaceMenu->setToolTipsVisible(true);
    findReplaceMenu->addAction(m_findReplaceAct);
    findReplaceMenu->addAction(m_findNextAct);
    findReplaceMenu->addAction(m_findPreviousAct);
    findReplaceMenu->addAction(m_replaceAct);
    findReplaceMenu->addAction(m_replaceFindAct);
    findReplaceMenu->addAction(m_replaceAllAct);
    findReplaceMenu->addSeparator();
    findReplaceMenu->addAction(searchedWordAct);
    searchedWordAct->setChecked(vconfig.getHighlightSearchedWord());

    m_findReplaceAct->setEnabled(false);
    m_findNextAct->setEnabled(false);
    m_findPreviousAct->setEnabled(false);
    m_replaceAct->setEnabled(false);
    m_replaceFindAct->setEnabled(false);
    m_replaceAllAct->setEnabled(false);

    editMenu->addSeparator();
    editMenu->addAction(expandTabAct);
    if (vconfig.getIsExpandTab()) {
        expandTabAct->setChecked(true);
    } else {
        expandTabAct->setChecked(false);
    }

    QMenu *tabStopWidthMenu = editMenu->addMenu(tr("Tab Stop Width"));
    tabStopWidthMenu->setToolTipsVisible(true);
    tabStopWidthMenu->addAction(twoSpaceTabAct);
    tabStopWidthMenu->addAction(fourSpaceTabAct);
    tabStopWidthMenu->addAction(eightSpaceTabAct);
    int tabStopWidth = vconfig.getTabStopWidth();
    switch (tabStopWidth) {
    case 2:
        twoSpaceTabAct->setChecked(true);
        break;
    case 4:
        fourSpaceTabAct->setChecked(true);
        break;
    case 8:
        eightSpaceTabAct->setChecked(true);
        break;
    default:
        qWarning() << "unsupported tab stop width" << tabStopWidth <<  "in config";
    }

    editMenu->addAction(m_autoIndentAct);
    m_autoIndentAct->setChecked(vconfig.getAutoIndent());

    editMenu->addAction(autoListAct);
    if (vconfig.getAutoList()) {
        // Let the trigger handler to trigger m_autoIndentAct, too.
        autoListAct->trigger();
    }
    Q_ASSERT(!(autoListAct->isChecked() && !m_autoIndentAct->isChecked()));

    editMenu->addAction(vimAct);
    vimAct->setChecked(vconfig.getEnableVimMode());

    editMenu->addAction(smartImAct);
    smartImAct->setChecked(vconfig.getEnableSmartImInVimMode());

    editMenu->addSeparator();

    initEditorStyleMenu(editMenu);

    initEditorBackgroundMenu(editMenu);

    editMenu->addAction(cursorLineAct);
    cursorLineAct->setChecked(vconfig.getHighlightCursorLine());

    editMenu->addAction(selectedWordAct);
    selectedWordAct->setChecked(vconfig.getHighlightSelectedWord());

    editMenu->addAction(trailingSapceAct);
    trailingSapceAct->setChecked(vconfig.getEnableTrailingSpaceHighlight());
}

void VMainWindow::initDockWindows()
{
    toolDock = new QDockWidget(tr("Tools"), this);
    toolDock->setObjectName("tools_dock");
    toolDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    toolBox = new QToolBox(this);
    outline = new VOutline(this);
    connect(editArea, &VEditArea::outlineChanged,
            outline, &VOutline::updateOutline);
    connect(outline, &VOutline::outlineItemActivated,
            editArea, &VEditArea::handleOutlineItemActivated);
    connect(editArea, &VEditArea::curHeaderChanged,
            outline, &VOutline::updateCurHeader);
    toolBox->addItem(outline, QIcon(":/resources/icons/outline.svg"), tr("Outline"));
    toolDock->setWidget(toolBox);
    addDockWidget(Qt::RightDockWidgetArea, toolDock);

    QAction *toggleAct = toolDock->toggleViewAction();
    toggleAct->setToolTip(tr("Toggle the tools dock widget"));
    viewMenu->addAction(toggleAct);
}

void VMainWindow::initAvatar()
{
    m_avatar = new VAvatar(this);
    m_avatar->setAvatarPixmap(":/resources/icons/vnote.svg");
    m_avatar->setColor(vnote->getColorFromPalette("base-color"), vnote->getColorFromPalette("Indigo4"),
                       vnote->getColorFromPalette("Teal4"));
    m_avatar->hide();
}

void VMainWindow::importNoteFromFile()
{
    static QString lastPath = QDir::homePath();
    QStringList files = QFileDialog::getOpenFileNames(this,
                                                      tr("Select Files (HTML or Markdown) To Import"),
                                                      lastPath);
    if (files.isEmpty()) {
        return;
    }
    // Update lastPath
    lastPath = QFileInfo(files[0]).path();

    int failedFiles = 0;
    for (int i = 0; i < files.size(); ++i) {
        bool ret = fileList->importFile(files[i]);
        if (!ret) {
            ++failedFiles;
        }
    }
    QMessageBox msgBox(QMessageBox::Information, tr("Import Notes From File"),
                       tr("Imported notes: %1 succeed, %2 failed.")
                       .arg(files.size() - failedFiles).arg(failedFiles),
                       QMessageBox::Ok, this);
    if (failedFiles > 0) {
        msgBox.setInformativeText(tr("Fail to import files maybe due to name conflicts."));
    }
    msgBox.exec();
}

void VMainWindow::changeMarkdownConverter(QAction *action)
{
    if (!action) {
        return;
    }

    MarkdownConverterType type = (MarkdownConverterType)action->data().toInt();

    qDebug() << "switch to converter" << type;

    vconfig.setMarkdownConverterType(type);
}

void VMainWindow::aboutMessage()
{
    QString info = tr("<span style=\"font-weight: bold;\">v%1</span>").arg(VConfigManager::c_version);
    info += "<br/><br/>";
    info += tr("VNote is a Vim-inspired note-taking application for Markdown.");
    info += "<br/>";
    info += tr("Please visit <a href=\"https://github.com/tamlok/vnote.git\">Github</a> for more information.");
    QMessageBox::about(this, tr("About VNote"), info);
}

void VMainWindow::changeExpandTab(bool checked)
{
    vconfig.setIsExpandTab(checked);
}

void VMainWindow::enableMermaid(bool p_checked)
{
    vconfig.setEnableMermaid(p_checked);
}

void VMainWindow::enableMathjax(bool p_checked)
{
    vconfig.setEnableMathjax(p_checked);
}

void VMainWindow::changeHighlightCursorLine(bool p_checked)
{
    vconfig.setHighlightCursorLine(p_checked);
}

void VMainWindow::changeHighlightSelectedWord(bool p_checked)
{
    vconfig.setHighlightSelectedWord(p_checked);
}

void VMainWindow::changeHighlightSearchedWord(bool p_checked)
{
    vconfig.setHighlightSearchedWord(p_checked);
}

void VMainWindow::changeHighlightTrailingSapce(bool p_checked)
{
    vconfig.setEnableTrailingSapceHighlight(p_checked);
}

void VMainWindow::setTabStopWidth(QAction *action)
{
    if (!action) {
        return;
    }
    vconfig.setTabStopWidth(action->data().toInt());
}

void VMainWindow::setEditorBackgroundColor(QAction *action)
{
    if (!action) {
        return;
    }

    vconfig.setCurBackgroundColor(action->data().toString());
}

void VMainWindow::initPredefinedColorPixmaps()
{
    const QVector<VColor> &bgColors = vconfig.getPredefinedColors();
    predefinedColorPixmaps.clear();
    int size = 256;
    for (int i = 0; i < bgColors.size(); ++i) {
        // Generate QPixmap filled in this color
        QColor color(VUtils::QRgbFromString(bgColors[i].rgb));
        QPixmap pixmap(size, size);
        pixmap.fill(color);
        predefinedColorPixmaps.append(pixmap);
    }
}

void VMainWindow::initRenderBackgroundMenu(QMenu *menu)
{
    QActionGroup *renderBackgroundAct = new QActionGroup(this);
    connect(renderBackgroundAct, &QActionGroup::triggered,
            this, &VMainWindow::setRenderBackgroundColor);

    QMenu *renderBgMenu = menu->addMenu(tr("&Rendering Background"));
    renderBgMenu->setToolTipsVisible(true);
    const QString &curBgColor = vconfig.getCurRenderBackgroundColor();
    QAction *tmpAct = new QAction(tr("System"), renderBackgroundAct);
    tmpAct->setToolTip(tr("Use system's background color configuration for Markdown rendering"));
    tmpAct->setCheckable(true);
    tmpAct->setData("System");
    if (curBgColor == "System") {
        tmpAct->setChecked(true);
    }
    renderBgMenu->addAction(tmpAct);

    const QVector<VColor> &bgColors = vconfig.getPredefinedColors();
    for (int i = 0; i < bgColors.size(); ++i) {
        tmpAct = new QAction(bgColors[i].name, renderBackgroundAct);
        tmpAct->setToolTip(tr("Set as the background color for Markdown rendering"));
        tmpAct->setCheckable(true);
        tmpAct->setData(bgColors[i].name);

#if !defined(Q_OS_MACOS) && !defined(Q_OS_MAC)
        tmpAct->setIcon(QIcon(predefinedColorPixmaps[i]));
#endif

        if (curBgColor == bgColors[i].name) {
            tmpAct->setChecked(true);
        }

        renderBgMenu->addAction(tmpAct);
    }
}

void VMainWindow::updateRenderStyleMenu()
{
    QMenu *menu = dynamic_cast<QMenu *>(sender());
    V_ASSERT(menu);

    QList<QAction *> actions = menu->actions();
    // Remove all other actions except the first one.
    for (int i = 1; i < actions.size(); ++i) {
        menu->removeAction(actions[i]);
        m_renderStyleActs->removeAction(actions[i]);
        delete actions[i];
    }

    // Update the menu actions with styles.
    QVector<QString> styles = vconfig.getCssStyles();
    for (auto const &style : styles) {
        QAction *act = new QAction(style, m_renderStyleActs);
        act->setToolTip(tr("Set as the CSS style for Markdown rendering"));
        act->setCheckable(true);
        act->setData(style);

        // Add it to the menu.
        menu->addAction(act);

        if (vconfig.getTemplateCss() == style) {
            act->setChecked(true);
        }
    }
}

void VMainWindow::initRenderStyleMenu(QMenu *p_menu)
{
    QMenu *styleMenu = p_menu->addMenu(tr("Rendering &Style"));
    styleMenu->setToolTipsVisible(true);
    connect(styleMenu, &QMenu::aboutToShow,
            this, &VMainWindow::updateRenderStyleMenu);

    m_renderStyleActs = new QActionGroup(this);
    connect(m_renderStyleActs, &QActionGroup::triggered,
            this, &VMainWindow::setRenderStyle);

    QAction *addAct = newAction(QIcon(":/resources/icons/add_style.svg"),
                                tr("&Add Style"), m_renderStyleActs);
    addAct->setToolTip(tr("Open the folder to add your custom CSS style files"));
    addAct->setCheckable(true);
    addAct->setData("AddStyle");

    styleMenu->addAction(addAct);
}

void VMainWindow::initEditorBackgroundMenu(QMenu *menu)
{
    QMenu *backgroundColorMenu = menu->addMenu(tr("&Background Color"));
    backgroundColorMenu->setToolTipsVisible(true);

    QActionGroup *backgroundColorAct = new QActionGroup(this);
    connect(backgroundColorAct, &QActionGroup::triggered,
            this, &VMainWindow::setEditorBackgroundColor);

    // System background color
    const QString &curBgColor = vconfig.getCurBackgroundColor();
    QAction *tmpAct = new QAction(tr("System"), backgroundColorAct);
    tmpAct->setToolTip(tr("Use system's background color configuration for editor"));
    tmpAct->setCheckable(true);
    tmpAct->setData("System");
    if (curBgColor == "System") {
        tmpAct->setChecked(true);
    }
    backgroundColorMenu->addAction(tmpAct);
    const QVector<VColor> &bgColors = vconfig.getPredefinedColors();
    for (int i = 0; i < bgColors.size(); ++i) {
        tmpAct = new QAction(bgColors[i].name, backgroundColorAct);
        tmpAct->setToolTip(tr("Set as the background color for editor"));
        tmpAct->setCheckable(true);
        tmpAct->setData(bgColors[i].name);
#if !defined(Q_OS_MACOS) && !defined(Q_OS_MAC)
        tmpAct->setIcon(QIcon(predefinedColorPixmaps[i]));
#endif
        if (curBgColor == bgColors[i].name) {
            tmpAct->setChecked(true);
        }

        backgroundColorMenu->addAction(tmpAct);
    }
}

void VMainWindow::updateEditorStyleMenu()
{
    QMenu *menu = dynamic_cast<QMenu *>(sender());
    V_ASSERT(menu);

    QList<QAction *> actions = menu->actions();
    // Remove all other actions except the first one.
    for (int i = 1; i < actions.size(); ++i) {
        menu->removeAction(actions[i]);
        m_editorStyleActs->removeAction(actions[i]);
        delete actions[i];
    }

    // Update the menu actions with styles.
    QVector<QString> styles = vconfig.getEditorStyles();
    for (auto const &style : styles) {
        QAction *act = new QAction(style, m_editorStyleActs);
        act->setToolTip(tr("Set as the editor style"));
        act->setCheckable(true);
        act->setData(style);

        // Add it to the menu.
        menu->addAction(act);

        if (vconfig.getEditorStyle() == style) {
            act->setChecked(true);
        }
    }
}

void VMainWindow::initEditorStyleMenu(QMenu *p_menu)
{
    QMenu *styleMenu = p_menu->addMenu(tr("Editor &Style"));
    styleMenu->setToolTipsVisible(true);
    connect(styleMenu, &QMenu::aboutToShow,
            this, &VMainWindow::updateEditorStyleMenu);

    m_editorStyleActs = new QActionGroup(this);
    connect(m_editorStyleActs, &QActionGroup::triggered,
            this, &VMainWindow::setEditorStyle);

    QAction *addAct = newAction(QIcon(":/resources/icons/add_style.svg"),
                                tr("&Add Style"), m_editorStyleActs);
    addAct->setToolTip(tr("Open the folder to add your custom MDHL style files"));
    addAct->setCheckable(true);
    addAct->setData("AddStyle");

    styleMenu->addAction(addAct);
}

void VMainWindow::setRenderBackgroundColor(QAction *action)
{
    if (!action) {
        return;
    }
    vconfig.setCurRenderBackgroundColor(action->data().toString());
    vnote->updateTemplate();
}

void VMainWindow::setRenderStyle(QAction *p_action)
{
    if (!p_action) {
        return;
    }

    QString data = p_action->data().toString();
    if (data == "AddStyle") {
        // Add custom style.
        QUrl url = QUrl::fromLocalFile(vconfig.getStyleConfigFolder());
        QDesktopServices::openUrl(url);
    } else {
        vconfig.setTemplateCss(data);
        vnote->updateTemplate();
    }
}

void VMainWindow::setEditorStyle(QAction *p_action)
{
    if (!p_action) {
        return;
    }

    QString data = p_action->data().toString();
    if (data == "AddStyle") {
        // Add custom style.
        QUrl url = QUrl::fromLocalFile(vconfig.getStyleConfigFolder());
        QDesktopServices::openUrl(url);
    } else {
        vconfig.setEditorStyle(data);
    }
}

void VMainWindow::updateActionStateFromTabStatusChange(const VFile *p_file,
                                                       bool p_editMode)
{
    m_printAct->setEnabled(p_file && p_file->getDocType() == DocType::Markdown);
    m_exportAsPDFAct->setEnabled(p_file && p_file->getDocType() == DocType::Markdown);

    editNoteAct->setVisible(p_file && p_file->isModifiable() && !p_editMode);
    discardExitAct->setVisible(p_file && p_editMode);
    saveExitAct->setVisible(p_file && p_editMode);
    saveNoteAct->setVisible(p_file && p_editMode);
    deleteNoteAct->setEnabled(p_file && p_file->isModifiable());
    noteInfoAct->setEnabled(p_file && p_file->getType() == FileType::Normal);

    m_insertImageAct->setEnabled(p_file && p_editMode);

    setActionsVisible(m_editToolBar, p_file && p_editMode);

    // Find/Replace
    m_findReplaceAct->setEnabled(p_file);
    m_findNextAct->setEnabled(p_file);
    m_findPreviousAct->setEnabled(p_file);
    m_replaceAct->setEnabled(p_file && p_editMode);
    m_replaceFindAct->setEnabled(p_file && p_editMode);
    m_replaceAllAct->setEnabled(p_file && p_editMode);

    if (!p_file) {
        m_findReplaceDialog->closeDialog();
    }
}

void VMainWindow::handleAreaTabStatusUpdated(const VEditTabInfo &p_info)
{
    bool editMode = false;
    m_curTab = p_info.m_editTab;
    if (m_curTab) {
        m_curFile = m_curTab->getFile();
        editMode = m_curTab->isEditMode();
    } else {
        m_curFile = NULL;
    }

    updateActionStateFromTabStatusChange(m_curFile, editMode);

    QString title;
    if (m_curFile) {
        m_findReplaceDialog->updateState(m_curFile->getDocType(), editMode);

        title = QString("[%1] %2").arg(m_curFile->getNotebookName()).arg(m_curFile->retrivePath());
        if (m_curFile->isModifiable()) {
            if (m_curFile->isModified()) {
                title.append('*');
            }
        } else {
            title.append('#');
        }
    }

    updateWindowTitle(title);
    updateStatusInfo(p_info);
}

void VMainWindow::onePanelView()
{
    changeSplitterView(1);
    expandViewAct->setChecked(false);
    m_onePanel = true;
}

void VMainWindow::twoPanelView()
{
    changeSplitterView(2);
    expandViewAct->setChecked(false);
    m_onePanel = false;
}

void VMainWindow::toggleOnePanelView()
{
    if (m_onePanel) {
        twoPanelView();
    } else {
        onePanelView();
    }
}

void VMainWindow::expandPanelView(bool p_checked)
{
    int nrSplits = 0;
    if (p_checked) {
        nrSplits = 0;
    } else {
        if (m_onePanel) {
            nrSplits = 1;
        } else {
            nrSplits = 2;
        }
    }
    changeSplitterView(nrSplits);
}

void VMainWindow::changeSplitterView(int nrPanel)
{
    switch (nrPanel) {
    case 0:
        // Expand
        mainSplitter->widget(0)->hide();
        mainSplitter->widget(1)->hide();
        mainSplitter->widget(2)->show();
        break;
    case 1:
        // Single panel
        mainSplitter->widget(0)->hide();
        mainSplitter->widget(1)->show();
        mainSplitter->widget(2)->show();
        break;
    case 2:
        // Two panels
        mainSplitter->widget(0)->show();
        mainSplitter->widget(1)->show();
        mainSplitter->widget(2)->show();
        break;
    default:
        qWarning() << "invalid panel number" << nrPanel;
    }
}

void VMainWindow::updateWindowTitle(const QString &str)
{
    QString title = "VNote";
    if (!str.isEmpty()) {
        title = title + " - " + str;
    }
    setWindowTitle(title);
}

void VMainWindow::curEditFileInfo()
{
    if (!m_curFile || m_curFile->getType() != FileType::Normal) {
        return;
    }
    fileList->fileInfo(m_curFile);
}

void VMainWindow::deleteCurNote()
{
    if (!m_curFile || !m_curFile->isModifiable()) {
        return;
    }
    fileList->deleteFile(m_curFile);
}

void VMainWindow::closeEvent(QCloseEvent *event)
{
    if (!editArea->closeAllFiles(false)) {
        // Fail to close all the opened files, cancel closing app
        event->ignore();
        return;
    }
    saveStateAndGeometry();
    QMainWindow::closeEvent(event);
}

void VMainWindow::saveStateAndGeometry()
{
    vconfig.setMainWindowGeometry(saveGeometry());
    vconfig.setMainWindowState(saveState());
    vconfig.setToolsDockChecked(toolDock->isVisible());
    vconfig.setMainSplitterState(mainSplitter->saveState());
}

void VMainWindow::restoreStateAndGeometry()
{
    const QByteArray &geometry = vconfig.getMainWindowGeometry();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }
    const QByteArray &state = vconfig.getMainWindowState();
    if (!state.isEmpty()) {
        restoreState(state);
    }
    toolDock->setVisible(vconfig.getToolsDockChecked());
    const QByteArray &splitterState = vconfig.getMainSplitterState();
    if (!splitterState.isEmpty()) {
        mainSplitter->restoreState(splitterState);
    }
}

const QVector<QPair<QString, QString> >& VMainWindow::getPalette() const
{
    return vnote->getPalette();
}

void VMainWindow::handleCurrentDirectoryChanged(const VDirectory *p_dir)
{
    newNoteAct->setEnabled(p_dir);
    m_importNoteAct->setEnabled(p_dir);
}

void VMainWindow::handleCurrentNotebookChanged(const VNotebook *p_notebook)
{
    newRootDirAct->setEnabled(p_notebook);
}

void VMainWindow::resizeEvent(QResizeEvent *event)
{
    repositionAvatar();
    QMainWindow::resizeEvent(event);
}

void VMainWindow::keyPressEvent(QKeyEvent *event)
{
    int key = event->key();
    Qt::KeyboardModifiers modifiers = event->modifiers();
    if (key == Qt::Key_Escape
        || (key == Qt::Key_BracketLeft
            && modifiers == Qt::ControlModifier)) {
        m_findReplaceDialog->closeDialog();
        event->accept();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

void VMainWindow::repositionAvatar()
{
    int diameter = mainSplitter->pos().y();
    int x = width() - diameter - 5;
    int y = 0;
    qDebug() << "avatar:" << diameter << x << y;
    m_avatar->setDiameter(diameter);
    m_avatar->move(x, y);
    m_avatar->show();
}

void VMainWindow::insertImage()
{
    if (!m_curTab) {
        return;
    }
    Q_ASSERT(m_curTab == editArea->currentEditTab());
    m_curTab->insertImage();
}

void VMainWindow::locateFile(VFile *p_file)
{
    if (!p_file || p_file->getType() != FileType::Normal) {
        return;
    }
    qDebug() << "locate file" << p_file->retrivePath();
    VNotebook *notebook = p_file->getNotebook();
    if (notebookSelector->locateNotebook(notebook)) {
        while (directoryTree->currentNotebook() != notebook) {
            QCoreApplication::sendPostedEvents();
        }
        VDirectory *dir = p_file->getDirectory();
        if (directoryTree->locateDirectory(dir)) {
            while (fileList->currentDirectory() != dir) {
                QCoreApplication::sendPostedEvents();
            }
            fileList->locateFile(p_file);
        }
    }

    // Open the directory and file panels after location.
    twoPanelView();
}

void VMainWindow::locateCurrentFile()
{
    if (m_curFile) {
        locateFile(m_curFile);
    }
}

void VMainWindow::handleFindDialogTextChanged(const QString &p_text, uint /* p_options */)
{
    bool enabled = true;
    if (p_text.isEmpty()) {
        enabled = false;
    }
    m_findNextAct->setEnabled(enabled);
    m_findPreviousAct->setEnabled(enabled);
    m_replaceAct->setEnabled(enabled);
    m_replaceFindAct->setEnabled(enabled);
    m_replaceAllAct->setEnabled(enabled);
}

void VMainWindow::openFindDialog()
{
    m_findReplaceDialog->openDialog(editArea->getSelectedText());
}

void VMainWindow::viewSettings()
{
    VSettingsDialog settingsDialog(this);
    settingsDialog.exec();
}

void VMainWindow::handleCaptainModeChanged(bool p_enabled)
{
    static QString normalBaseColor = m_avatar->getBaseColor();
    static QString captainModeColor = vnote->getColorFromPalette("Purple5");

    if (p_enabled) {
        m_avatar->updateBaseColor(captainModeColor);
    } else {
        m_avatar->updateBaseColor(normalBaseColor);
    }
}

void VMainWindow::closeCurrentFile()
{
    if (m_curFile) {
        editArea->closeFile(m_curFile, false);
    }
}

void VMainWindow::changeAutoIndent(bool p_checked)
{
    vconfig.setAutoIndent(p_checked);
}

void VMainWindow::changeAutoList(bool p_checked)
{
    vconfig.setAutoList(p_checked);
    if (p_checked) {
        if (!m_autoIndentAct->isChecked()) {
            m_autoIndentAct->trigger();
        }
        m_autoIndentAct->setEnabled(false);
    } else {
        m_autoIndentAct->setEnabled(true);
    }
}

void VMainWindow::changeVimMode(bool p_checked)
{
    vconfig.setEnableVimMode(p_checked);
}

void VMainWindow::enableCodeBlockHighlight(bool p_checked)
{
    vconfig.setEnableCodeBlockHighlight(p_checked);
}

void VMainWindow::enableImagePreview(bool p_checked)
{
    vconfig.setEnablePreviewImages(p_checked);
}

void VMainWindow::enableImagePreviewConstraint(bool p_checked)
{
    vconfig.setEnablePreviewImageConstraint(p_checked);
}

void VMainWindow::enableImageConstraint(bool p_checked)
{
    vconfig.setEnableImageConstraint(p_checked);

    vnote->updateTemplate();
}

void VMainWindow::enableImageCaption(bool p_checked)
{
    vconfig.setEnableImageCaption(p_checked);
}

void VMainWindow::shortcutHelp()
{
    QString locale = VUtils::getLocale();
    QString docName = VNote::c_shortcutsDocFile_en;
    if (locale == "zh_CN") {
        docName = VNote::c_shortcutsDocFile_zh;
    }
    VFile *file = vnote->getOrphanFile(docName);
    editArea->openFile(file, OpenFileMode::Read);
}

void VMainWindow::printNote()
{
    QPrinter printer;
    QPrintDialog dialog(&printer, this);
    dialog.setWindowTitle(tr("Print Note"));

    V_ASSERT(m_curTab);

    if (m_curFile->getDocType() == DocType::Markdown) {
        VMdTab *mdTab = dynamic_cast<VMdTab *>((VEditTab *)m_curTab);
        VWebView *webView = mdTab->getWebViewer();

        V_ASSERT(webView);

        if (webView->hasSelection()) {
            dialog.addEnabledOption(QAbstractPrintDialog::PrintSelection);
        }

        if (dialog.exec() != QDialog::Accepted) {
            return;
        }
    }
}

void VMainWindow::exportAsPDF()
{
    V_ASSERT(m_curTab);
    V_ASSERT(m_curFile);

    if (m_curFile->getDocType() == DocType::Markdown) {
        VMdTab *mdTab = dynamic_cast<VMdTab *>((VEditTab *)m_curTab);
        VExporter exporter(mdTab->getMarkdownConverterType(), this);
        exporter.exportNote(m_curFile, ExportType::PDF);
        exporter.exec();
    }
}

QAction *VMainWindow::newAction(const QIcon &p_icon,
                                const QString &p_text,
                                QObject *p_parent)
{
#if defined(Q_OS_MACOS) || defined(Q_OS_MAC)
    Q_UNUSED(p_icon);
    return new QAction(p_text, p_parent);
#else
    return new QAction(p_icon, p_text, p_parent);
#endif
}

void VMainWindow::showStatusMessage(const QString &p_msg)
{
    const int timeout = 3000;
    statusBar()->showMessage(p_msg, timeout);
}

void VMainWindow::updateStatusInfo(const VEditTabInfo &p_info)
{
    if (m_curTab) {
        m_tabIndicator->update(p_info);
        m_tabIndicator->show();

        if (m_curTab->isEditMode()) {
            m_curTab->requestUpdateVimStatus();
        } else {
            m_vimIndicator->hide();
        }
    } else {
        m_tabIndicator->hide();
        m_vimIndicator->hide();
    }
}

void VMainWindow::handleVimStatusUpdated(const VVim *p_vim)
{
    if (!p_vim || !m_curTab || !m_curTab->isEditMode()) {
        m_vimIndicator->hide();
    } else {
        m_vimIndicator->update(p_vim);
        m_vimIndicator->show();
    }
}

#ifndef EDITORLIST_H
#define EDITORLIST_H

#include <QTabWidget>
#include <QSplitter>
#include <QWidget>
#include "utils.h"

class Editor;
class EditorList
{
public:
    enum ShowType{
        lstNone,
        lstLeft,
        lstRight,
        lstBoth
    };

    explicit EditorList(QTabWidget* leftPageWidget,
                        QTabWidget* rightPageWidget,
                        QSplitter* splitter,
                        QWidget* panel);

    Editor* NewEditor(const QString& filename, FileEncodingType encoding,
                     bool inProject, bool newFile,
                     QTabWidget* page=NULL);

    Editor* GetEditor(int index=-1, QTabWidget* tabsWidget=NULL) const;

    bool CloseEditor(Editor* editor, bool transferFocus=true, bool force=false);

private:
    QTabWidget* GetNewEditorPageControl();


private:
    ShowType mLayout;
    QTabWidget *mLeftPageWidget;
    QTabWidget *mRightPageWidget;
    QSplitter *mSplitter;
    QWidget *mPanel;
    int mUpdateCount;



};

#endif // EDITORLIST_H

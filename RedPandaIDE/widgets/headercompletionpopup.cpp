/*
 * Copyright (C) 2020-2022 Roy Qu (royqh1979@gmail.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "headercompletionpopup.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QVBoxLayout>
#include <QDebug>
#include "../utils.h"

HeaderCompletionPopup::HeaderCompletionPopup(QWidget* parent):QWidget(parent)
{
    setWindowFlags(Qt::Popup);
    mListView = new CodeCompletionListView(this);
    mModel=new HeaderCompletionListModel(&mCompletionList);
    QItemSelectionModel *m=mListView->selectionModel();
    mListView->setModel(mModel);
    delete m;
    setLayout(new QVBoxLayout());
    layout()->addWidget(mListView);
    layout()->setMargin(0);

    mSearchLocal = false;
    mCurrentFile = "";
    mPhrase = "";
    mIgnoreCase = false;
}

HeaderCompletionPopup::~HeaderCompletionPopup()
{
    delete mModel;
}

void HeaderCompletionPopup::prepareSearch(const QString &phrase, const QString &fileName)
{
    QCursor oldCursor = cursor();
    setCursor(Qt::WaitCursor);
    mCurrentFile = fileName;
    mPhrase = phrase;
    getCompletionFor(phrase);
    setCursor(oldCursor);
}

bool HeaderCompletionPopup::search(const QString &phrase, bool autoHideOnSingleResult)
{
    mPhrase = phrase;
    if (mPhrase.isEmpty()) {
        hide();
        return false;
    }
    if(!isEnabled())
        return false;

    QCursor oldCursor = cursor();
    setCursor(Qt::WaitCursor);

    int i = mPhrase.lastIndexOf('\\');
    if (i<0) {
        i = mPhrase.lastIndexOf('/');
    }
    QString symbol = mPhrase;
    if (i>=0) {
        symbol = mPhrase.mid(i+1);
    }

    // filter fFullCompletionList to fCompletionList
    filterList(symbol);
    mModel->notifyUpdated();
    setCursor(oldCursor);

    if (!mCompletionList.isEmpty()) {
        mListView->setCurrentIndex(mModel->index(0,0));
        if (mCompletionList.count() == 1) {
            // if only one suggestion and auto hide , don't show the frame
            if (autoHideOnSingleResult)
                return true;
            // if only one suggestion, and is exactly the symbol to search, hide the frame (the search is over)
            if (symbol == mCompletionList[0]->filename)
                return true;
        }
    } else {
        hide();
    }
    return false;
}

void HeaderCompletionPopup::setKeypressedCallback(const KeyPressedCallback &newKeypressedCallback)
{
    mListView->setKeypressedCallback(newKeypressedCallback);
}

void HeaderCompletionPopup::setSuggestionColor(const QColor& localColor,
                                               const QColor& projectColor,
                                               const QColor& systemColor,
                                               const QColor& folderColor)
{
    mModel->setLocalColor(localColor);
    mModel->setProjectColor(projectColor);
    mModel->setSystemColor(systemColor);
    mModel->setFolderColor(folderColor);
}

QString HeaderCompletionPopup::selectedFilename(bool updateUsageCount)
{
    if (!isEnabled())
        return "";
    int index = mListView->currentIndex().row();
    PHeaderCompletionListItem item;
    if (index>=0 && index<mCompletionList.count())
        item=mCompletionList[index];
    else if (mCompletionList.count()>0) {
        item=mCompletionList.front();
    }
    if (item) {
        if (updateUsageCount) {
            item->usageCount++;
            mHeaderUsageCounts.insert(item->fullpath,item->usageCount);
        }
        if (item->isFolder)
            return item->filename+"/";
        return item->filename;
    }
    return "";
}

static bool sortByUsage(const PHeaderCompletionListItem& item1,const PHeaderCompletionListItem& item2){
    if (item1->usageCount != item2->usageCount)
        return item1->usageCount > item2->usageCount;

    if (item1->itemType != item2->itemType)
        return item1->itemType<item2->itemType;

    return item1->filename < item2->filename;
}


void HeaderCompletionPopup::filterList(const QString &member)
{
    Qt::CaseSensitivity caseSensitivity=mIgnoreCase?Qt::CaseInsensitive:Qt::CaseSensitive;
    mCompletionList.clear();
    if (member.isEmpty()) {
        foreach (const PHeaderCompletionListItem& item,mFullCompletionList.values()) {
            mCompletionList.append(item);
        }
    } else {
        foreach (const PHeaderCompletionListItem& item,mFullCompletionList.values()) {
            if (item->filename.startsWith(member, caseSensitivity)) {
                mCompletionList.append(item);
            }
        }
    }
    std::sort(mCompletionList.begin(),mCompletionList.end(), sortByUsage);
}


void HeaderCompletionPopup::getCompletionFor(const QString &phrase)
{
    int idx = phrase.lastIndexOf('\\');
    if (idx<0) {
        idx = phrase.lastIndexOf('/');
    }
    mFullCompletionList.clear();
    if (idx < 0) { // dont have basedir
        if (mSearchLocal) {
            QFileInfo fileInfo(mCurrentFile);
            addFilesInPath(fileInfo.absolutePath(), HeaderCompletionListItemType::LocalHeader);
        };

        for (const QString& path: mParser->includePaths()) {
            addFilesInPath(path, HeaderCompletionListItemType::ProjectHeader);
        }

        for (const QString& path: mParser->projectIncludePaths()) {
            addFilesInPath(path, HeaderCompletionListItemType::SystemHeader);
        }
    } else {
        QString current = phrase.mid(0,idx);
        if (mSearchLocal) {
            QFileInfo fileInfo(mCurrentFile);
            addFilesInSubDir(fileInfo.absolutePath(),current, HeaderCompletionListItemType::LocalHeader);
        }
        for (const QString& path: mParser->includePaths()) {
            addFilesInSubDir(path,current, HeaderCompletionListItemType::ProjectHeader);
        }

        for (const QString& path: mParser->projectIncludePaths()) {
            addFilesInSubDir(path,current, HeaderCompletionListItemType::SystemHeader);
        }
    }
}

void HeaderCompletionPopup::addFilesInPath(const QString &path, HeaderCompletionListItemType type)
{
    QDir dir(path);
    if (!dir.exists())
        return;
    foreach (const QFileInfo& fileInfo, dir.entryInfoList()) {
        if (fileInfo.fileName().startsWith("."))
            continue;
        if (fileInfo.isDir()) {
            addFile(dir, fileInfo, type);
            continue;
        }
        QString suffix = fileInfo.suffix().toLower();
        if (suffix == "h" || suffix == "hpp" || suffix == "") {
            addFile(dir, fileInfo, type);
        }
    }
}

void HeaderCompletionPopup::addFile(const QDir& dir, const QFileInfo& fileInfo, HeaderCompletionListItemType type)
{
    QString fileName = fileInfo.fileName();
    if (fileName.isEmpty())
        return;
    if (fileName.startsWith('.'))
        return;
    PHeaderCompletionListItem item = std::make_shared<HeaderCompletionListItem>();
    item->filename = fileName;
    item->itemType = type;
    item->fullpath = cleanPath(dir.absoluteFilePath(fileName));
    item->usageCount = mHeaderUsageCounts.value(item->fullpath,0);
    item->isFolder = fileInfo.isDir();
    mFullCompletionList.insert(fileName,item);
}

void HeaderCompletionPopup::addFilesInSubDir(const QString &baseDirPath, const QString &subDirName, HeaderCompletionListItemType type)
{
    QDir baseDir(baseDirPath);
    QString subDirPath = baseDir.filePath(subDirName);
    addFilesInPath(subDirPath, type);
}

bool HeaderCompletionPopup::searchLocal() const
{
    return mSearchLocal;
}

void HeaderCompletionPopup::setSearchLocal(bool newSearchLocal)
{
    mSearchLocal = newSearchLocal;
}

bool HeaderCompletionPopup::ignoreCase() const
{
    return mIgnoreCase;
}

void HeaderCompletionPopup::setIgnoreCase(bool newIgnoreCase)
{
    mIgnoreCase = newIgnoreCase;
}

const QString &HeaderCompletionPopup::phrase() const
{
    return mPhrase;
}

void HeaderCompletionPopup::setParser(const PCppParser &newParser)
{
    mParser = newParser;
}

void HeaderCompletionPopup::showEvent(QShowEvent *)
{
    mListView->setFocus();
}

void HeaderCompletionPopup::hideEvent(QHideEvent *)
{
    mCompletionList.clear();
    mFullCompletionList.clear();
    mParser = nullptr;
}

bool HeaderCompletionPopup::event(QEvent *event)
{
    bool result = QWidget::event(event);
    switch (event->type()) {
    case QEvent::FontChange:
        mListView->setFont(font());
        break;
    default:
        break;
    }
    return result;
}

HeaderCompletionListModel::HeaderCompletionListModel(const QList<PHeaderCompletionListItem> *files, QObject *parent):
    QAbstractListModel(parent),
    mFiles(files)
{

}

int HeaderCompletionListModel::rowCount(const QModelIndex &) const
{
    return mFiles->count();
}

QVariant HeaderCompletionListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();
    if (index.row()>=mFiles->count())
        return QVariant();

    switch(role) {
    case Qt::DisplayRole: {
        return mFiles->at(index.row())->filename;
        }
    case Qt::ForegroundRole: {
        PHeaderCompletionListItem item=mFiles->at(index.row());
        if (item->isFolder)
            return mFolderColor;
        switch(item->itemType) {
        case HeaderCompletionListItemType::LocalHeader:
            return mLocalColor;
        case HeaderCompletionListItemType::ProjectHeader:
            return mProjectColor;
        case HeaderCompletionListItemType::SystemHeader:
            return mSystemColor;
        }
    }
        break;
    }
    return QVariant();
}

void HeaderCompletionListModel::notifyUpdated()
{
    beginResetModel();
    endResetModel();
}

void HeaderCompletionListModel::setSystemColor(const QColor &newColor)
{
    mSystemColor = newColor;
}

void HeaderCompletionListModel::setProjectColor(const QColor &newColor)
{
    mProjectColor = newColor;
}

void HeaderCompletionListModel::setFolderColor(const QColor &newFolderColor)
{
    mFolderColor = newFolderColor;
}

void HeaderCompletionListModel::setLocalColor(const QColor &newColor)
{
    mLocalColor = newColor;
}

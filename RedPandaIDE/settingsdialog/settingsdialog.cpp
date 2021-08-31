#include "settingsdialog.h"
#include "ui_settingsdialog.h"
#include "settingswidget.h"
#include "compilersetoptionwidget.h"
#include "editorgeneralwidget.h"
#include "editorfontwidget.h"
#include "editorclipboardwidget.h"
#include "editorcolorschemewidget.h"
#include "editorcodecompletionwidget.h"
#include "editorsyntaxcheckwidget.h"
#include "editorsymbolcompletionwidget.h"
#include "editorautosavewidget.h"
#include "editormiscwidget.h"
#include "environmentappearencewidget.h"
#include "executorgeneralwidget.h"
#include "debuggeneralwidget.h"
#include "formattergeneralwidget.h"
#include <QDebug>
#include <QMessageBox>
#include <QModelIndex>

SettingsDialog::SettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);

    ui->widgetsView->setModel(&model);

    model.setHorizontalHeaderLabels(QStringList());

    ui->btnApply->setEnabled(false);

    pEnvironmentAppearenceWidget = new EnvironmentAppearenceWidget(tr("Appearence"),tr("Environment"));
    pEnvironmentAppearenceWidget->init();
    addWidget(pEnvironmentAppearenceWidget);

    pCompilerSetOptionWidget = new CompilerSetOptionWidget(tr("Compiler Set"),tr("Compiler"));
    pCompilerSetOptionWidget->init();
    addWidget(pCompilerSetOptionWidget);

    pEditorGeneralWidget = new EditorGeneralWidget(tr("General"),tr("Editor"));
    pEditorGeneralWidget->init();
    addWidget(pEditorGeneralWidget);

    pEditorFontWidget = new EditorFontWidget(tr("Font"),tr("Editor"));
    pEditorFontWidget->init();
    addWidget(pEditorFontWidget);

    pEditorClipboardWidget = new EditorClipboardWidget(tr("Copy & Export"),tr("Editor"));
    pEditorClipboardWidget->init();
    addWidget(pEditorClipboardWidget);

    pEditorColorSchemeWidget = new EditorColorSchemeWidget(tr("Color"),tr("Editor"));
    pEditorColorSchemeWidget->init();
    addWidget(pEditorColorSchemeWidget);

    pEditorCodeCompletionWidget = new EditorCodeCompletionWidget(tr("Code Completion"),tr("Editor"));
    pEditorCodeCompletionWidget->init();
    addWidget(pEditorCodeCompletionWidget);

    pEditorSymbolCompletionWidget = new EditorSymbolCompletionWidget(tr("Symbol Completion"),tr("Editor"));
    pEditorSymbolCompletionWidget->init();
    addWidget(pEditorSymbolCompletionWidget);

    pEditorSyntaxCheckWidget = new EditorSyntaxCheckWidget(tr("Auto Syntax Checking"),tr("Editor"));
    pEditorSyntaxCheckWidget->init();
    addWidget(pEditorSyntaxCheckWidget);

    pEditorAutoSaveWidget = new EditorAutoSaveWidget(tr("Auto save"),tr("Editor"));
    pEditorAutoSaveWidget->init();
    addWidget(pEditorAutoSaveWidget);

    pEditorMiscWidget = new EditorMiscWidget(tr("Misc"),tr("Editor"));
    pEditorMiscWidget->init();
    addWidget(pEditorMiscWidget);


    pExecutorGeneralWidget = new ExecutorGeneralWidget(tr("General"),tr("Program Runner"));
    pExecutorGeneralWidget->init();
    addWidget(pExecutorGeneralWidget);

    pDebugGeneralWidget = new DebugGeneralWidget(tr("General"),tr("Debugger"));
    pDebugGeneralWidget->init();
    addWidget(pDebugGeneralWidget);

    pFormatterGeneralWidget = new FormatterGeneralWidget(tr("General"),tr("Code Formatter"));
    pFormatterGeneralWidget->init();
    addWidget(pFormatterGeneralWidget);


    ui->widgetsView->expandAll();
    //select the first widget of the first group
    auto groupIndex = ui->widgetsView->model()->index(0,0);
    auto widgetIndex = ui->widgetsView->model()->index(0,0, groupIndex);
    ui->widgetsView->selectionModel()->setCurrentIndex(
                widgetIndex,
                QItemSelectionModel::Select
                );
    on_widgetsView_clicked(widgetIndex);
}

SettingsDialog::~SettingsDialog()
{
    for (SettingsWidget* p:mSettingWidgets) {
        p->setParent(nullptr);
        delete p;
    }
    delete ui;
}

void SettingsDialog::addWidget(SettingsWidget *pWidget)
{
    QList<QStandardItem*> items = model.findItems(pWidget->group());
    QStandardItem* pGroupItem;
    if (items.count() == 0 ) {
        pGroupItem = new QStandardItem(pWidget->group());
        pGroupItem->setData(-1, GetWidgetIndexRole);
        model.appendRow(pGroupItem);
    } else {
        pGroupItem = items[0];
    }
    mSettingWidgets.append(pWidget);
    QStandardItem* pWidgetItem = new QStandardItem(pWidget->name());
    pWidgetItem->setData(mSettingWidgets.count()-1, GetWidgetIndexRole);
    pGroupItem->appendRow(pWidgetItem);
    connect(pWidget, &SettingsWidget::settingsChanged,
                                this , &SettingsDialog::widget_settings_changed);
}


void SettingsDialog::on_widgetsView_clicked(const QModelIndex &index)
{
    if (!index.isValid())
        return;
    int i = index.data(GetWidgetIndexRole).toInt();
    if (i>=0) {
        saveCurrentPageSettings(true);
        SettingsWidget* pWidget = mSettingWidgets[i];
        if (ui->scrollArea->widget()!=nullptr) {
            QWidget* w = ui->scrollArea->takeWidget();
            w->setParent(nullptr);
        }
        ui->scrollArea->setWidget(pWidget);
        ui->lblWidgetCaption->setText(QString("%1 > %2").arg(pWidget->group()).arg(pWidget->name()));

        ui->btnApply->setEnabled(false);
    } else if (model.hasChildren(index)) {
        ui->widgetsView->expand(index);
        QModelIndex childIndex = this->model.index(0,0,index);
        emit ui->widgetsView->clicked(childIndex);
    }
}

void SettingsDialog::widget_settings_changed(bool value)
{
    ui->btnApply->setEnabled(value);
}

void SettingsDialog::on_btnCancle_pressed()
{
    this->close();
}

void SettingsDialog::on_btnApply_pressed()
{
    saveCurrentPageSettings(false);
}

void SettingsDialog::on_btnOk_pressed()
{
    saveCurrentPageSettings(false);
    this->close();
}

void SettingsDialog::saveCurrentPageSettings(bool confirm)
{
    if (ui->scrollArea->widget()==ui->scrollAreaWidgetContents)
        return;
    SettingsWidget* pWidget = (SettingsWidget*) ui->scrollArea->widget();
    if (!pWidget->isSettingsChanged())
        return;
    if (confirm) {
        if (QMessageBox::warning(this,tr("Save Changes"),
               tr("There are changes in the settings, do you want to save them before swtich to other page?"),
               QMessageBox::Yes, QMessageBox::No)!=QMessageBox::Yes) {
            return;
        }
    }
    pWidget->save();
}

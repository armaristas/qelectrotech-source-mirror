// SPDX-License-Identifier: GPL-2.0-or-later
#include "../TerminalStrip/GraphicsItem/terminalstripitem.h"
#include "../TerminalStrip/UndoCommand/addterminaltostripcommand.h"
#include "../TerminalStrip/physicalterminal.h"
#include "../TerminalStrip/realterminal.h"
#include "../TerminalStrip/terminalcolor.h"
#include "../TerminalStrip/terminalstrip.h"
#include "../TerminalStrip/terminalstripautoimport.h"
#include "../TerminalStrip/terminalstripbridge.h"
#include "../diagram.h"
#include "../diagramview.h"
#include "../projectview.h"
#include "../qetapp.h"
#include "../qetdiagrameditor.h"
#include "../qetproject.h"
#include "../undocommand/addgraphicsobjectcommand.h"
#include "gneradortb.h"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QCollator>
#include <QComboBox>
#include <QCursor>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QStyleOptionButton>
#include <QStyledItemDelegate>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>
#include <QToolBar>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QtMath>
#include <algorithm>
#include <functional>
#include <memory>

namespace Faramax::TB
{
namespace
{
struct Column
{
    const char *field;
    const char *title;
    int kind = 0;
}; // text, bool, integer, read-only
const QVector<Column> columns{{"corte", "Corte", 1},
                              {"posicion", "Posición", 3},
                              {"tipo_borna", "Tipo\nBorna"},
                              {"modelo", "Modelo\nBorna"},
                              {"color", "Color"},
                              {"disp_ext", "Disp.\nExt"},
                              {"mang_ext", "Manguera\nExt"},
                              {"cable_ext", "Cable\nExt"},
                              {"puente", "Puente", 1},
                              {"bornero", "Bornero", 3},
                              {"num_borna", "Nº\nBorna"},
                              {"peine", "Peine", 1},
                              {"cable_int", "Cable\nInt"},
                              {"mang_int", "Manguera\nInt"},
                              {"disp_int", "Disp.\nInt"},
                              {"reserva", "Reserva", 1},
                              {"nivel", "Nivel", 2},
                              {"shl_ext", "SHL\nExt", 1},
                              {"mang_shl_ext", "Rango\nSHL Ext"},
                              {"shl_int", "SHL\nInt", 1},
                              {"mang_shl_int", "Rango\nSHL Int"},
                              {"peine_grupo", "Grupo\nPeine", 2}};
int fieldColumn(const QString &field)
{
    for (int i = 0; i < columns.size(); ++i)
        if (field == columns[i].field)
            return i;
    return -1;
}
constexpr int SwatchRole = Qt::UserRole + 10;
constexpr int StripeRole = Qt::UserRole + 11;
const QMap<QString, QString> captions{
    {"enabled", "Usar la presentación configurada"},
    {"drawing_style", "Tipo de bornero"},
    {"x_start", "Origen X"},
    {"y_base", "Origen Y"},
    {"paso_x", "Paso entre bornas"},
    {"hose_delim", "Delimitador de manguera"},
    {"borna_delim", "Delimitador de borna"},
    {"char_adv_ext", "Avance mínimo de texto"},
    {"offset_ext", "Desplazamiento de destino externo"},
    {"offset_int", "Desplazamiento de destino interno"},
    {"line_borna_len", "Longitud de salida de borna"},
    {"line_manguera_len", "Longitud de tramo de manguera"},
    {"line_dispositivo_len", "Longitud de tramo de dispositivo"},
    {"cable_text_offset_int", "Desplazamiento texto cable interno"},
    {"hose_text_offset_int", "Desplazamiento texto manguera interna"},
    {"cable_text_offset_ext", "Desplazamiento texto cable externo"},
    {"hose_text_offset_ext", "Desplazamiento texto manguera externa"},
    {"cable_line_len_ext", "Marca cable externo"},
    {"cable_line_len_int", "Marca cable interno"},
    {"hose_line_len_ext", "Marca manguera externa"},
    {"hose_line_len_int", "Marca manguera interna"},
    {"open_after_export", "Abrir Excel después de exportar"},
    {"draw_ground_line", "Dibujar línea de tierra"},
    {"draw_frame", "Marco exterior del bornero"},
    {"font_family", "Fuente general"},
    {"font_family_ext", "Fuente externa"},
    {"font_family_int", "Fuente interna"},
    {"font_size_ext", "Tamaño de fuente externa"},
    {"font_size_int", "Tamaño de fuente interna"},
    {"font_bold_ext", "Negrita externa"},
    {"font_bold_int", "Negrita interna"},
    {"hose_font_family", "Fuente de mangueras"},
    {"hose_font_size", "Tamaño de fuente de mangueras"},
    {"hose_font_bold", "Negrita de mangueras"},
    {"hose_group_mode", "Agrupación de mangueras (Normal / Compacta)"},
    {"enable_multilevel", "Representar multinivel"},
    {"draw_shield_symbol", "Dibujar pantallas SHL"},
    {"reserve_count", "Reservas automáticas al final del bornero"},
    {"reserve_label_prefix", "Prefijo de reservas"},
    {"auto_detect_reserves", "Detectar bornas sin conexión como reservas"},
    {"external_name_exceptions", "Reglas INT/EXT: prefijo*|INT o prefijo*|EXT"},
    {"enable_terminal_colors", "Colorear bornas"},
    {"terminal_default_color", "Color predeterminado"},
    {"terminal_color_rules_json", "Reglas de color: lista JSON de text / color"},
    {"terminal_color_gray_codes", "Códigos grises (reglas antiguas)"},
    {"terminal_color_blue_codes", "Códigos azules (reglas antiguas)"},
    {"terminal_color_red_codes", "Códigos rojos (reglas antiguas)"},
    {"terminal_color_black_codes", "Códigos negros (reglas antiguas)"},
    {"terminal_color_pe_codes", "Códigos PE (reglas antiguas)"},
    {"multilevel_sep", "Separador de niveles"}};
QString text(const QJsonValue &v)
{
    return v.isBool()     ? (v.toBool() ? QStringLiteral("1") : QStringLiteral("0"))
           : v.isDouble() ? QString::number(v.toDouble())
                          : v.toString();
}
class Choices final : public QStyledItemDelegate
{
  public:
    mutable QPointer<QWidget> active;
    std::function<void()> finished;
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        auto color = index.data(SwatchRole).value<QColor>();
        if (color.isValid())
        {
            painter->save();
            const auto box = option.rect.adjusted(1, 1, -1, -1);
            painter->fillRect(box, color);
            auto stripe = index.data(StripeRole).value<QColor>();
            if (stripe.isValid())
                painter->fillRect(QRect(box.center().x(), box.top(), box.width() / 2, box.height()), stripe);
            painter->setPen(
                QPen(option.state & QStyle::State_Selected ? QColor("#5ad7f7") : QColor("#8c9ca6"),
                     option.state & QStyle::State_Selected ? 2 : 1));
            painter->drawRect(box);
            painter->restore();
            return;
        }
        if (index.data(Qt::CheckStateRole).isValid())
        {
            QStyleOptionViewItem cell(option);
            initStyleOption(&cell, index);
            cell.features &= ~QStyleOptionViewItem::HasCheckIndicator;
            cell.text.clear();
            auto *style = option.widget ? option.widget->style() : QApplication::style();
            style->drawControl(QStyle::CE_ItemViewItem, &cell, painter, option.widget);
            const auto caption = index.data().toString();
            const int size = 16;
            QStyleOptionViewItem check(cell);
            check.rect =
                QRect(option.rect.center().x() - size / 2, option.rect.center().y() - size / 2, size, size);
            if (!caption.isEmpty())
                check.rect.moveRight(option.rect.right() - 5);
            check.state &= ~(QStyle::State_On | QStyle::State_Off);
            check.state |=
                index.data(Qt::CheckStateRole).toInt() == Qt::Checked ? QStyle::State_On : QStyle::State_Off;
            if (!(index.flags() & Qt::ItemIsUserCheckable))
                check.state &= ~QStyle::State_Enabled;
            style->drawPrimitive(QStyle::PE_IndicatorItemViewItemCheck, &check, painter, option.widget);
            if (check.state & QStyle::State_On)
            {
                painter->save();
                painter->setPen(QPen(QColor("#172b35"), 1.6));
                painter->drawLine(check.rect.topLeft() + QPoint(3, 8), check.rect.topLeft() + QPoint(7, 12));
                painter->drawLine(check.rect.topLeft() + QPoint(7, 12), check.rect.topLeft() + QPoint(13, 4));
                painter->restore();
            }
            if (!caption.isEmpty())
            {
                painter->save();
                painter->setPen(index.data(Qt::ForegroundRole).value<QBrush>().color());
                painter->drawText(option.rect.adjusted(4, 0, -24, 0), Qt::AlignVCenter | Qt::AlignLeft,
                                  caption);
                painter->restore();
            }
            return;
        }
        QStyledItemDelegate::paint(painter, option, index);
    }
    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option,
                     const QModelIndex &index) override
    {
        if (index.data(Qt::CheckStateRole).isValid())
        {
            if (!(index.flags() & Qt::ItemIsUserCheckable))
                return false;
            if (event->type() == QEvent::MouseButtonRelease &&
                static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton)
            {
                return model->setData(index,
                                      index.data(Qt::CheckStateRole).toInt() == Qt::Checked ? Qt::Unchecked
                                                                                            : Qt::Checked,
                                      Qt::CheckStateRole);
            }
            if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick)
                return true;
        }
        return QStyledItemDelegate::editorEvent(event, model, option, index);
    }
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override
    {
        auto choices = index.data(Qt::UserRole).toStringList();
        QWidget *widget = nullptr;
        if (choices.isEmpty())
            widget = QStyledItemDelegate::createEditor(parent, option, index);
        else
        {
            auto *combo = new QComboBox(parent);
            combo->setEditable(true);
            combo->addItems(choices);
            widget = combo;
        }
        active = widget;
        if (widget)
            connect(widget, &QObject::destroyed, const_cast<Choices *>(this),
                    [this]()
                    {
                        if (finished)
                            finished();
                    });
        return widget;
    }
    void setEditorData(QWidget *widget, const QModelIndex &index) const override
    {
        if (auto *combo = qobject_cast<QComboBox *>(widget))
            combo->setCurrentText(index.data().toString());
        else
            QStyledItemDelegate::setEditorData(widget, index);
    }
    void setModelData(QWidget *widget, QAbstractItemModel *model, const QModelIndex &index) const override
    {
        if (auto *combo = qobject_cast<QComboBox *>(widget))
            model->setData(index, combo->currentText());
        else
            QStyledItemDelegate::setModelData(widget, model, index);
    }
};
class Editor final : public QDialog
{
    QPointer<QETProject> project;
    QPointer<TerminalStrip> strip;
    QComboBox *strips = nullptr; // Native active-strip identity, shared by sidebar and commands.
    QListWidget *stripList = nullptr;
    QListWidget *exceptions = nullptr;
    QLineEdit *exceptionInput = nullptr;
    QComboBox *exceptionSide = nullptr;
    QLabel *tableSummary = nullptr;
    QPlainTextEdit *messages = nullptr;
    QTableWidget *table = nullptr;
    QLabel *status = nullptr;
    QLineEdit *search = nullptr;
    QPlainTextEdit *diagnostics = nullptr;
    QTabWidget *tabs = nullptr;
    QWidget *configPage = nullptr;
    QGraphicsView *preview = nullptr;
    QGraphicsScene *scene = nullptr;
    Choices *delegate = nullptr;
    QVector<Row> data;
    QJsonObject cfg, loadedConfig;
    bool loading = false, configDirty = false, applying = false;
    bool refreshPending = false;
    QTimer liveTimer;
    QDialog *configDialog = nullptr;
    bool keepConfig = false;
    bool painting = false, paintValue = false;
    int paintColumn = -1, paintFirst = -1, paintLast = -1;
    int moveSource = -1;
    bool paintSticky = false, resizeOnRelease = false;
    QVector<QMetaObject::Connection> connections;
    QMap<QUuid, QByteArray> exported;

  public:
    Editor(QETProject *p, TerminalStrip *s, QWidget *parent) : QDialog(parent), project(p)
    {
        setAttribute(Qt::WA_DeleteOnClose);
        setWindowTitle(QStringLiteral("GNerador TB · Gestor de borneros"));
        setObjectName("faramaxGNeradorTB");
        setWindowFlags(windowFlags() | Qt::WindowMinMaxButtonsHint);
        resize(1600, 900);
        setMinimumSize(1000, 650);
        setStyleSheet(QStringLiteral(R"CSS(
QDialog#TBConfiguration { background:#071b24; }
QDialog#faramaxGNeradorTB { background:#071b24; color:#eaf7ff; font-family:'Segoe UI'; font-size:12px; }
QWidget { color:#eaf7ff; font-family:'Segoe UI'; font-size:12px; }
QFrame#TBLeft, QFrame#TBWork { background:#0b2534; border:1px solid #25465a; border-radius:6px; }
QLabel { background:transparent; }
QScrollArea, QWidget#TBSettingsPage { background:#071b24; border:0; }
QGroupBox { background:#0b2534; border:1px solid #25465a; border-radius:5px; margin-top:14px; padding:14px 10px 10px; }
QGroupBox::title { subcontrol-origin:margin; left:12px; padding:0 5px; color:#5ad7f7; font-weight:600; }
QLabel#TBTitle { font-size:15px; font-weight:700; }
QLabel#TBSection { font-weight:700; }
QLabel#TBTableSummary { color:#8db4c6; font-size:11px; }
QPushButton { background:#102b3c; border:1px solid #2d5264; border-radius:6px; padding:6px 8px; font-weight:600; }
QPushButton:hover { background:#15384d; border-color:#5ad7f7; }
QPushButton:disabled { color:#64818e; }


QPushButton#TBInsert { background:#299b68; border-color:#54c68e; }
QPushButton#TBClose { background:#3c1f22; border-color:#75383e; }
QLineEdit,QPlainTextEdit,QListWidget,QComboBox,QDoubleSpinBox { background:#081a24; color:#eaf7ff; border:1px solid #25465a; border-radius:4px; padding:4px; selection-background-color:#146a94; }
QListWidget::item { min-height:26px; padding:1px 3px; }
QListWidget::item:selected { background:#146a94; }
QTableWidget { background:#071820; alternate-background-color:#0d2634; color:#eaf7ff; gridline-color:#1d3d50; selection-background-color:#146a94; selection-color:white; border:1px solid #25465a; font-size:11px; }
QHeaderView { background:#102b3c; }
QHeaderView::section { background:#102b3c; color:#d9eef8; border:1px solid #25465a; padding:4px; font-weight:700; font-size:11px; }
QTableCornerButton::section { background:#102b3c; border:1px solid #25465a; }
QTableView::indicator { width:16px; height:16px; border:1px solid #64818e; border-radius:4px; background:#132f3a; }
QTableView::indicator:checked { background:#e68bec; border-color:#ecb5ef; image:none; }
QToolBar { background:#0b2534; border:0; spacing:3px; }
QToolButton { background:#102b3c; border:1px solid #25465a; border-radius:3px; padding:4px; }
QToolButton:hover { background:#146a94; }
QTabWidget::pane { border:0; }
QTabBar::tab { background:#102b3c; color:#8db4c6; padding:5px 10px; }
QTabBar::tab:selected { color:white; border-bottom:2px solid #42b883; }
QMenu { background:#0b2534; color:#eaf7ff; border:1px solid #25465a; }
QMenu::item:selected { background:#146a94; }
QScrollBar:vertical,QScrollBar:horizontal { background:#081a24; border:0; }
QScrollBar::handle:vertical,QScrollBar::handle:horizontal { background:#25465a; border-radius:4px; min-height:24px; min-width:24px; }
QSplitter::handle { background:#071b24; }
QToolTip { background:#102b3c; color:#eaf7ff; border:1px solid #5ad7f7; }
)CSS"));
        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(6, 6, 6, 6);
        layout->setSpacing(6);
        auto *splitter = new QSplitter(Qt::Horizontal, this);
        layout->addWidget(splitter);
        auto *side = new QFrame(splitter);
        side->setObjectName("TBLeft");
        side->setMinimumWidth(225);
        side->setMaximumWidth(340);
        auto *left = new QVBoxLayout(side);
        left->setContentsMargins(7, 7, 7, 7);
        left->setSpacing(5);
        auto *title = new QLabel(QStringLiteral("GNerador_TB · QET"), side);
        title->setObjectName("TBTitle");
        left->addWidget(title);
        auto button = [this, left, side](const QString &label, const char *name, auto action)
        {
            auto *b = new QPushButton(label, side);
            b->setObjectName(name);
            left->addWidget(b);
            connect(b, &QPushButton::clicked, this, action);
            return b;
        };
        button(QStringLiteral("Insertar plan en folio"), "TBInsert",
               [this]()
               {
                   insert();
               });
        button(QStringLiteral("Guardar Excel / historial"), "TBExcel",
               [this]()
               {
                   workbook();
               });
        button(QStringLiteral("Configuración"), "TBConfig",
               [this]()
               {
                   configDialog->show();
                   configDialog->raise();
                   configDialog->activateWindow();
               });
        button(QStringLiteral("Cerrar"), "TBClose",
               [this]()
               {
                   close();
               });
        left->addWidget(new QLabel(QStringLiteral("Borneros encontrados:"), side));
        strips = new QComboBox(this);
        strips->hide();
        stripList = new QListWidget(side);
        stripList->setObjectName("TBStripList");
        stripList->setMinimumHeight(100);
        left->addWidget(stripList, 2);
        connect(stripList, &QListWidget::currentRowChanged, this,
                [this](int row)
                {
                    if (loading || row < 0)
                        return;
                    strips->setCurrentIndex(row);
                    if (strips->currentIndex() != row)
                    {
                        QSignalBlocker block(stripList);
                        stripList->setCurrentRow(strips->currentIndex());
                    }
                });
        left->addWidget(new QLabel(QStringLiteral("Excepciones EXT/INT:"), side));
        auto *entry = new QHBoxLayout;
        exceptionInput = new QLineEdit(side);
        exceptionInput->setObjectName("TBExceptionPattern");
        exceptionInput->setPlaceholderText(QStringLiteral("XCC*, DGA, PT100…"));
        entry->addWidget(exceptionInput, 1);
        exceptionSide = new QComboBox(side);
        exceptionSide->addItems({"EXT", "INT"});
        entry->addWidget(exceptionSide);
        left->addLayout(entry);
        button(QStringLiteral("Añadir excepción"), "TBAddException",
               [this]()
               {
                   addException();
               });
        exceptions = new QListWidget(side);
        exceptions->setObjectName("TBExceptions");
        exceptions->setMaximumHeight(110);
        left->addWidget(exceptions, 1);
        auto *removeException = new QPushButton(QStringLiteral("Quitar excepción seleccionada"), side);
        left->addWidget(removeException);
        connect(removeException, &QPushButton::clicked, this,
                [this]()
                {
                    if (exceptions->currentRow() < 0)
                        return;
                    delete exceptions->takeItem(exceptions->currentRow());
                    saveExceptions();
                });
        connect(exceptionInput, &QLineEdit::returnPressed, this,
                [this]()
                {
                    addException();
                });
        left->addWidget(new QLabel(QStringLiteral("Buscar:"), side));
        search = new QLineEdit(side);
        search->setObjectName("TBSearch");
        search->setPlaceholderText(QStringLiteral("Filtrar tabla (cualquier columna)…"));
        left->addWidget(search);
        left->addWidget(new QLabel(QStringLiteral("Mensajes:"), side));
        messages = new QPlainTextEdit(side);
        messages->setObjectName("TBMessages");
        messages->setReadOnly(true);
        messages->setMinimumHeight(130);
        left->addWidget(messages, 3);
        auto *work = new QFrame(splitter);
        work->setObjectName("TBWork");
        auto *right = new QVBoxLayout(work);
        right->setContentsMargins(7, 7, 7, 7);
        right->setSpacing(4);
        auto *heading = new QHBoxLayout;
        auto *caption = new QLabel(QStringLiteral("Tabla de bornas"), work);
        caption->setObjectName("TBSection");
        heading->addWidget(caption);
        tableSummary = new QLabel(work);
        tableSummary->setObjectName("TBTableSummary");
        tableSummary->setTextInteractionFlags(Qt::TextSelectableByMouse);
        heading->addWidget(tableSummary, 1, Qt::AlignRight);
        right->addLayout(heading);
        splitter->setStretchFactor(0, 0);
        splitter->setStretchFactor(1, 1);
        splitter->setSizes({240, 1360});
        auto *tools = new QToolBar(this);
        tools->hide();
        tools->addAction(QStringLiteral("Asignar / reagrupar"), this,
                         [this]()
                         {
                             if (!cleanForAction())
                                 return;
                             TerminalStripAutoImport::importFreeTerminals(project, true);
                             populate(strips->currentData().toUuid());
                         });
        tools->addAction(QStringLiteral("Añadir reserva"), this,
                         [this]()
                         {
                             add(false);
                         });
        tools->addAction(QStringLiteral("Duplicar como reserva"), this,
                         [this]()
                         {
                             add(true);
                         });
        tools->addAction(QStringLiteral("Reservas manuales"), this,
                         [this]()
                         {
                             if (!writable() || !cleanForAction())
                                 return;
                             int converted = 0;
                             for (int i : selectedRows())
                                 if (data[i].values.value("auto_reserve").toBool())
                                 {
                                     data[i].edits["auto_reserve"] = false;
                                     data[i].values["auto_reserve"] = false;
                                     ++converted;
                                 }
                             if (!converted)
                                 return;
                             cfg["reserve_count"] = qMax(0, cfg.value("reserve_count").toInt() - converted);
                             configDirty = true;
                             queueApply(300);
                             apply();
                         });
        tools->addAction(QStringLiteral("Quitar del bornero"), this,
                         [this]()
                         {
                             remove();
                         });
        tools->addAction(QStringLiteral("↑"), this,
                         [this]()
                         {
                             move(-1);
                         });
        tools->addAction(QStringLiteral("↓"), this,
                         [this]()
                         {
                             move(1);
                         });
        tools->addAction(QStringLiteral("Multinivel"), this,
                         [this]()
                         {
                             expand();
                         });
        tools->addAction(QStringLiteral("Tamaño del peine…"), this,
                         [this]()
                         {
                             combSize();
                         });
        tools->addAction(QStringLiteral("Ordenar"), this,
                         [this]()
                         {
                             if (!writable() || !cleanForAction())
                                 return;
                             QCollator compare;
                             compare.setNumericMode(true);
                             auto ordered = data;
                             std::stable_sort(ordered.begin(), ordered.end(),
                                              [&compare](const Row &a, const Row &b)
                                              {
                                                  return compare.compare(a.values["num_borna"].toString(),
                                                                         b.values["num_borna"].toString()) <
                                                         0;
                                              });
                             if (auto *command = orderCommand(strip, ordered))
                             {
                                 project->undoStack()->push(command);
                                 load(strip);
                             }
                         });
        tools->addAction(QStringLiteral("Rellenar abajo"), this,
                         [this]()
                         {
                             fill(false);
                         });
        tools->addAction(QStringLiteral("Rellenar derecha"), this,
                         [this]()
                         {
                             fill(true);
                         });
        tools->addAction(QStringLiteral("Copiar"), this,
                         [this]()
                         {
                             copy(false);
                         });
        tools->addAction(QStringLiteral("Cortar"), this,
                         [this]()
                         {
                             copy(true);
                         });
        tools->addAction(QStringLiteral("Pegar"), this,
                         [this]()
                         {
                             paste();
                         });
        tools->addAction(QStringLiteral("Excel / historial"), this,
                         [this]()
                         {
                             workbook();
                         });
        tools->addAction(QStringLiteral("Recuperar historial"), this,
                         [this]()
                         {
                             restore();
                         });
        auto *columnMenu = new QMenu(this);
        auto *columnButton = tools->addAction(QStringLiteral("Columnas"));
        columnButton->setMenu(columnMenu);
        configDialog = new QDialog(this);
        configDialog->setObjectName("TBConfiguration");
        configDialog->setWindowTitle(QStringLiteral("Configuración del bornero"));
        configDialog->resize(850, 750);
        auto *configLayout = new QVBoxLayout(configDialog);
        status = new QLabel(this);
        status->setWordWrap(true);
        left->insertWidget(7, status);
        status->setMaximumHeight(72);
        tabs = new QTabWidget(configDialog);
        configLayout->addWidget(tabs);
        table = new QTableWidget(this);
        table->setObjectName("gneradorTBTable");
        table->setContextMenuPolicy(Qt::CustomContextMenu);
        table->viewport()->installEventFilter(this);
        table->installEventFilter(this);
        connect(table, &QWidget::customContextMenuRequested, this,
                [this](const QPoint &point)
                {
                    contextMenu(point);
                });
        table->setColumnCount(columns.size());
        table->setAlternatingRowColors(true);
        table->setWordWrap(false);
        table->verticalHeader()->setDefaultSectionSize(30);
        table->horizontalHeader()->setMinimumHeight(50);
        const QVector<int> widths{50, 70, 84, 85,  58, 100, 74, 74, 58, 67, 64,
                                  58, 74, 74, 105, 64, 52,  58, 76, 58, 76, 68};
        for (int c = 0; c < widths.size(); ++c)
            table->setColumnWidth(c, widths[c]);
        table->setColumnHidden(fieldColumn("peine_grupo"), true);
        table->setSelectionMode(QAbstractItemView::ExtendedSelection);
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
        table->horizontalHeader()->setStretchLastSection(true);
        delegate = new Choices(table);
        delegate->finished = [this]()
        {
            QTimer::singleShot(0, this,
                               [this]()
                               {
                                   if (refreshPending && !loading && !applying)
                                       requestReload();
                               });
        };
        table->setItemDelegate(delegate);
        auto shortcut = [this](const QKeySequence &key, auto callback)
        {
            auto *action = new QAction(table);
            action->setShortcut(key);
            action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
            table->addAction(action);
            connect(action, &QAction::triggered, this, callback);
        };
        shortcut(QKeySequence::Copy,
                 [this]()
                 {
                     copy(false);
                 });
        shortcut(QKeySequence::Cut,
                 [this]()
                 {
                     copy(true);
                 });
        shortcut(QKeySequence::Paste,
                 [this]()
                 {
                     paste();
                 });
        shortcut(QKeySequence::Delete,
                 [this]()
                 {
                     clearCells();
                 });
        shortcut(QKeySequence::Undo,
                 [this]()
                 {
                     if (cleanForAction())
                         project->undoStack()->undo();
                 });
        shortcut(QKeySequence::Redo,
                 [this]()
                 {
                     if (cleanForAction())
                         project->undoStack()->redo();
                 });
        shortcut(QKeySequence::SelectAll,
                 [this]()
                 {
                     table->selectAll();
                 });
        shortcut(QKeySequence("Ctrl+D"),
                 [this]()
                 {
                     fill(false);
                 });
        shortcut(QKeySequence("Ctrl+R"),
                 [this]()
                 {
                     fill(true);
                 });
        shortcut(QKeySequence("Ctrl+Shift+Delete"),
                 [this]()
                 {
                     remove();
                 });
        QStringList headers;
        for (int c = 0; c < columns.size(); ++c)
        {
            headers << QString::fromUtf8(columns[c].title).replace(' ', '\n');
            auto *a = columnMenu->addAction(QString::fromUtf8(columns[c].title).replace('\n', ' '));
            a->setCheckable(true);
            a->setChecked(c != fieldColumn("peine_grupo"));
            connect(a, &QAction::toggled, this,
                    [this, c](bool show)
                    {
                        table->setColumnHidden(c, !show);
                    });
        }
        table->setHorizontalHeaderLabels(headers);
        right->addWidget(table, 1);
        auto *scroll = new QScrollArea(this);
        scroll->setWidgetResizable(true);
        configPage = new QWidget(scroll);
        configPage->setObjectName("TBSettingsPage");
        scroll->setWidget(configPage);
        tabs->addTab(scroll, QStringLiteral("Presentación y reglas"));
        diagnostics = new QPlainTextEdit(this);
        diagnostics->setReadOnly(true);
        tabs->addTab(diagnostics, QStringLiteral("Comprobaciones"));
        scene = new QGraphicsScene(this);
        preview = new QGraphicsView(scene, this);
        preview->setRenderHint(QPainter::Antialiasing);
        tabs->addTab(preview, QStringLiteral("Vista previa"));
        auto *operations = new QWidget(configDialog);
        auto *operationsLayout = new QGridLayout(operations);
        int actionIndex = 0;
        for (auto *action : tools->actions())
        {
            auto *control = new QPushButton(action->text(), operations);
            control->setMenu(action->menu());
            if (!action->menu())
                connect(control, &QPushButton::clicked, action, &QAction::trigger);
            operationsLayout->addWidget(control, actionIndex / 2, actionIndex % 2);
            ++actionIndex;
        }
        auto *json = new QPushButton(QStringLiteral("Importar / exportar configuración JSON"), operations);
        operationsLayout->addWidget(json, actionIndex / 2 + 1, 0, 1, 2);
        connect(json, &QPushButton::clicked, this,
                [this]()
                {
                    configuration();
                });
        tabs->addTab(operations, QStringLiteral("Edición y datos"));
        auto *configClose = new QDialogButtonBox(QDialogButtonBox::Close, configDialog);
        configLayout->addWidget(configClose);
        connect(configClose, &QDialogButtonBox::rejected, configDialog, &QDialog::hide);
        liveTimer.setSingleShot(true);
        connect(&liveTimer, &QTimer::timeout, this,
                [this]()
                {
                    if (delegate->active || painting)
                    {
                        liveTimer.start(50);
                        return;
                    }
                    if (dirty())
                        apply(false);
                });
        connect(table, &QTableWidget::itemChanged, this,
                [this](QTableWidgetItem *item)
                {
                    if (loading || item->row() >= data.size())
                        return;
                    const int c = item->column();
                    const auto col = columns[c];
                    if (col.kind == 3)
                        return;
                    QJsonValue value = col.kind == 1   ? QJsonValue(item->checkState() == Qt::Checked)
                                       : col.kind == 2 ? QJsonValue(item->text().toInt())
                                                       : QJsonValue(item->text());
                    if (QString(col.field) == "tipo_borna")
                        value = nativeType(value.toString());
                    if (QString(col.field) == "color")
                    {
                        const QMap<QString, QString> names{
                            {"Gris", "gray"},     {"Azul", "blue"},   {"Verde/Amarillo", "green/yellow"},
                            {"Rojo", "red"},      {"Negro", "black"}, {"Blanco", "white"},
                            {"Naranja", "orange"}};
                        value = names.value(value.toString(), value.toString());
                        QColor fill, stripe;
                        auto spec = value.toString();
                        spec.replace('|', '/');
                        if (spec.compare("Auto", Qt::CaseInsensitive) != 0 &&
                            TerminalColor::parse(spec, &fill, &stripe))
                            value = TerminalColor::toSpec(fill, stripe);
                    }
                    auto &r = data[item->row()];
                    r.values.insert(col.field, value);
                    r.edits.insert(col.field, value);
                    if (QString(col.field) == "peine" && value.toBool() &&
                        r.values.value("peine_grupo").toInt() == 0)
                    {
                        r.values["peine_grupo"] = 1;
                        r.edits["peine_grupo"] = 1;
                    }
                    decorate();
                    queueApply();
                });
        connect(search, &QLineEdit::textChanged, this,
                [this]()
                {
                    filter();
                });
        connect(tabs, &QTabWidget::currentChanged, this,
                [this](int index)
                {
                    if (index == 1)
                        diagnostics->setPlainText(validate(data, cfg).join('\n'));
                    if (index == 2)
                        drawPreview();
                });
        connect(strips, qOverload<int>(&QComboBox::currentIndexChanged), this,
                [this](int)
                {
                    if (loading)
                        return;
                    if (dirty() && !apply(false))
                    {
                        QSignalBlocker block(strips);
                        strips->setCurrentIndex(strips->findData(strip ? strip->uuid() : QUuid()));
                        status->setText(
                            QStringLiteral("Corrige los valores pendientes antes de cambiar de bornero."));
                        return;
                    }
                    {
                        QSignalBlocker block(stripList);
                        stripList->setCurrentRow(strips->currentIndex());
                    }
                    load(selectedStrip());
                });
        connect(p, &QObject::destroyed, this,
                [this]()
                {
                    configDirty = false;
                    data.clear();
                    close();
                });
        connect(p->undoStack(), &QUndoStack::indexChanged, this,
                [this]()
                {
                    QTimer::singleShot(0, this,
                                       [this]()
                                       {
                                           requestReload();
                                       });
                });
        populate(s ? s->uuid() : QUuid());
        for (auto *button : findChildren<QPushButton *>())
        {
            button->setAutoDefault(false);
            button->setDefault(false);
        }
        watch(p);
    }
    void setDraft(int row, const QString &field, const QJsonValue &value)
    {
        if (row < 0 || row >= data.size())
            return;
        data[row].values[field] = value;
        data[row].edits[field] = value;
        QSignalBlocker block(table);
        auto *item = table->item(row, fieldColumn(field));
        if (item)
        {
            if (value.isBool())
                item->setCheckState(value.toBool() ? Qt::Checked : Qt::Unchecked);
            else
                item->setText(text(value));
        }
    }
    void combSize()
    {
        const int row = table->currentRow();
        if (row < 0 || !writable())
            return;
        int anchor = row;
        while (anchor > 0 && data[anchor].values.value("peine_grupo").toInt() < 0)
            --anchor;
        bool accepted = false;
        const int count = QInputDialog::getInt(
            this, QStringLiteral("Tamaño del peine"),
            QStringLiteral("Posiciones del tramo (0 elimina el peine). Marca después sus contactos."),
            qMax(1, data[anchor].values.value("peine_grupo").toInt()), 0, qMin(10, int(data.size()) - anchor),
            1, &accepted);
        if (accepted)
            setCombSpan(anchor, count);
    }
    void setCombSpan(int row, int count)
    {
        if (row < 0 || row >= data.size())
            return;
        count = qBound(0, count, qMin(10, int(data.size()) - row));
        const int old = qMax(0, data[row].values.value("peine_grupo").toInt());
        for (int i = row; i < qMin(int(data.size()), row + qMax(1, qMax(old, count))); ++i)
        {
            if (i == row && count > 0)
            {
                setDraft(i, "peine_grupo", count);
                if (old == 0)
                    setDraft(i, "peine", true);
            }
            else if (i < row + count)
                setDraft(i, "peine_grupo", -1);
            else
            {
                setDraft(i, "peine_grupo", 0);
                setDraft(i, "peine", false);
            }
        }
        decorate();
        queueApply();
    }
    void paintCell(int row)
    {
        if (row < 0 || row >= data.size() || table->isRowHidden(row))
            return;
        if (paintColumn == fieldColumn("puente") && paintValue)
        {
            int next = row + 1;
            while (next < data.size() && data[next].terminal->level() != data[row].terminal->level())
                ++next;
            if (next == data.size())
                return;
        }
        if (paintColumn == fieldColumn("peine") && data[row].values.value("peine_grupo").toInt() == 0)
            setDraft(row, "peine_grupo", 1);
        setDraft(row, columns[paintColumn].field, paintValue);
        decorate();
    }
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == table || watched == table->viewport())
        {
            if (event->type() == QEvent::KeyPress && static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape)
            {
                painting = false;
                paintSticky = false;
                moveSource = -1;
                table->viewport()->unsetCursor();
                queueApply();
                return true;
            }
            if (watched == table->viewport() && project && !project->isReadOnly())
            {
                if (event->type() == QEvent::MouseButtonPress)
                {
                    auto *mouse = static_cast<QMouseEvent *>(event);
                    auto index = table->indexAt(mouse->position().toPoint());
                    if (moveSource >= 0 && index.isValid())
                    {
                        moveBelow(moveSource, index.row());
                        moveSource = -1;
                        table->viewport()->unsetCursor();
                        return true;
                    }
                    if (mouse->button() == Qt::LeftButton && index.isValid() &&
                        (index.column() == fieldColumn("puente") || index.column() == fieldColumn("peine")))
                    {
                        liveTimer.stop();
                        painting = true;
                        paintFirst = paintLast = index.row();
                        if (!paintSticky)
                        {
                            paintColumn = index.column();
                            paintValue = !data[paintFirst].values.value(columns[paintColumn].field).toBool();
                        }
                        if (index.column() != paintColumn)
                        {
                            painting = false;
                            return false;
                        }
                        resizeOnRelease = !paintSticky && paintColumn == fieldColumn("peine") &&
                                          data[paintFirst].values.value("peine_grupo").toInt() > 0;
                        if (!resizeOnRelease)
                            paintCell(paintFirst);
                        return true;
                    }
                }
                if (event->type() == QEvent::MouseMove && painting)
                {
                    auto *mouse = static_cast<QMouseEvent *>(event);
                    auto index = table->indexAt(mouse->position().toPoint());
                    if (index.isValid() && index.column() == paintColumn && index.row() != paintLast)
                    {
                        if (resizeOnRelease)
                        {
                            resizeOnRelease = false;
                            paintValue = true;
                            paintCell(paintFirst);
                        }
                        const int direction = index.row() > paintLast ? 1 : -1;
                        for (int row = paintLast + direction; row != index.row() + direction;
                             row += direction)
                            paintCell(row);
                        paintLast = index.row();
                    }
                    return true;
                }
                if (event->type() == QEvent::MouseButtonRelease && painting)
                {
                    painting = false;
                    if (resizeOnRelease)
                    {
                        resizeOnRelease = false;
                        setCombSpan(paintFirst, data[paintFirst].values.value("peine_grupo").toInt() + 1);
                    }
                    else
                        queueApply();
                    return true;
                }
            }
        }
        return QDialog::eventFilter(watched, event);
    }
    void moveBelow(int source, int target)
    {
        if (!cleanForAction() || source < 0 || target < 0 || source >= data.size() || target >= data.size() ||
            source == target)
            return;
        auto ordered = data;
        auto row = ordered.takeAt(source);
        ordered.insert(target - (source < target ? 1 : 0) + 1, row);
        if (auto *command = orderCommand(strip, ordered))
            project->undoStack()->push(command);
    }
    void insertRow(int target)
    {
        if (!writable() || !cleanForAction())
            return;
        QSet<QString> labels;
        for (const auto &r : data)
            labels.insert(r.terminal->label());
        QString label;
        for (int i = 1;; ++i)
        {
            label = strip->name() + cfg.value("borna_delim").toString() + "R" + QString::number(i);
            if (!labels.contains(label))
                break;
        }
        ElementData info;
        info.m_informations.addValue("label", label);
        info.m_informations.addValue("manufacturer_reference", "RESERVA");
        const int position =
            target >= data.size() ? strip->physicalTerminalCount() : data[qMax(0, target)].physical->pos();
        project->undoStack()->push(
            new AddSpareTerminalsCommand(strip, position, {RealTerminal::createSpare(info)}));
    }
    void contextMenu(const QPoint &point)
    {
        const auto index = table->indexAt(point);
        if (!index.isValid())
            return;
        if (!table->item(index.row(), index.column())->isSelected())
        {
            table->clearSelection();
            table->setCurrentCell(index.row(), index.column());
        }
        else
            table->setCurrentCell(index.row(), index.column(), QItemSelectionModel::NoUpdate);
        QMenu menu(this);
        menu.setObjectName("TBEssentialMenu");
        if (index.column() == fieldColumn("puente") || index.column() == fieldColumn("peine"))
        {
            const QString noun = index.column() == fieldColumn("puente")
                                     ? QStringLiteral("puentes")
                                     : QStringLiteral("contactos de peine");
            for (bool on : {true, false})
                menu.addAction(
                    (on ? QStringLiteral("Pintar ") : QStringLiteral("Borrar ")) + noun, this,
                    [this, on, index]()
                    {
                        paintColumn = index.column();
                        paintValue = on;
                        paintSticky = true;
                        table->viewport()->setCursor(Qt::CrossCursor);
                        status->setText(QStringLiteral(
                            "Arrastra sobre la columna para pintar. Esc termina el modo de pintura."));
                    });
            menu.addSeparator();
        }
        menu.addAction(QStringLiteral("Copiar"), this,
                       [this]()
                       {
                           copy(false);
                       });
        menu.addAction(QStringLiteral("Pegar"), this,
                       [this]()
                       {
                           paste();
                       });
        menu.addAction(QStringLiteral("Cortar"), this,
                       [this]()
                       {
                           copy(true);
                       });
        menu.addSeparator();
        menu.addAction(QStringLiteral("Rellenar hacia abajo"), this,
                       [this]()
                       {
                           fill(false);
                       });
        auto *rowsMenu = menu.addMenu(QStringLiteral("Fila (Borna)"));
        rowsMenu->addAction(QStringLiteral("Insertar arriba"), this,
                            [this, index]()
                            {
                                insertRow(index.row());
                            });
        rowsMenu->addAction(QStringLiteral("Insertar abajo"), this,
                            [this, index]()
                            {
                                insertRow(index.row() + 1);
                            });
        rowsMenu->addAction(QStringLiteral("Mover debajo de…"), this,
                            [this, index]()
                            {
                                moveSource = index.row();
                                table->viewport()->setCursor(Qt::CrossCursor);
                                status->setText(QStringLiteral("Selecciona la fila destino. Esc cancela."));
                            });
        rowsMenu->addAction(QStringLiteral("Duplicar como reserva"), this,
                            [this]()
                            {
                                add(true);
                            });
        rowsMenu->addAction(QStringLiteral("Quitar del bornero"), this,
                            [this]()
                            {
                                remove();
                            });
        menu.addSeparator();
        menu.addAction(QStringLiteral("Subir"), this,
                       [this]()
                       {
                           move(-1);
                       });
        menu.addAction(QStringLiteral("Bajar"), this,
                       [this]()
                       {
                           move(1);
                       });
        menu.addSeparator();
        menu.addAction(QStringLiteral("Tamaño peine…"), this,
                       [this]()
                       {
                           combSize();
                       });
        menu.addAction(QStringLiteral("Multinivel…"), this,
                       [this]()
                       {
                           expand();
                       });
        menu.exec(table->viewport()->mapToGlobal(point));
    }
    void loadExceptions()
    {
        QSignalBlocker block(exceptions);
        exceptions->clear();
        for (auto rule : cfg.value("external_name_exceptions")
                             .toString()
                             .split(QRegularExpression("[\n,;]+"), Qt::SkipEmptyParts))
        {
            rule = rule.trimmed().toUpper();
            rule.replace("=>", "|");
            if (!rule.contains('|'))
                rule += "|EXT";
            auto *item =
                new QListWidgetItem(rule.section('|', 0, 0) + "    " + rule.section('|', 1, 1), exceptions);
            item->setData(Qt::UserRole, rule);
        }
    }
    void saveExceptions()
    {
        if (loading)
            return;
        QStringList rules;
        for (int i = 0; i < exceptions->count(); ++i)
            rules << exceptions->item(i)->data(Qt::UserRole).toString();
        cfg["external_name_exceptions"] = rules.join('\n');
        configDirty = true;
        buildConfig();
        refreshDraft();
        queueApply();
        status->setText(QStringLiteral("Excepciones INT/EXT actualizadas en el proyecto."));
    }
    void addException()
    {
        if (!strip)
            return;
        const auto parts = exceptionInput->text().split(QRegularExpression("[,;\\s]+"), Qt::SkipEmptyParts);
        for (auto pattern : parts)
        {
            pattern = pattern.toUpper();
            const auto rule = pattern + "|" + exceptionSide->currentText();
            bool exists = false;
            for (int i = 0; i < exceptions->count(); ++i)
                exists |= exceptions->item(i)->data(Qt::UserRole).toString() == rule;
            if (!exists)
            {
                auto *item = new QListWidgetItem(pattern + "    " + exceptionSide->currentText(), exceptions);
                item->setData(Qt::UserRole, rule);
            }
        }
        if (parts.isEmpty())
            return;
        exceptionInput->clear();
        saveExceptions();
    }
    void refreshDraft()
    {
        if (loading || !strip)
            return;
        const auto current = rows(strip, cfg);
        QMap<QString, QJsonObject> values;
        for (const auto &r : current)
            values[r.id] = r.values;
        for (auto &r : data)
            if (values.contains(r.id))
            {
                r.values = values.value(r.id);
                for (auto it = r.edits.begin(); it != r.edits.end(); ++it)
                    r.values[it.key()] = it.value();
            }
        const auto position = table->verticalScrollBar()->value();
        fillTable();
        filter();
        decorate();
        table->verticalScrollBar()->setValue(position);
        if (tabs->currentIndex() == 2)
            drawPreview();
    }
    void decorate()
    {
        if (!table || !messages)
            return;
        QSignalBlocker block(table);
        const QColor green("#98e49a"), orange("#edb88d"), red("#ff6666"), purple("#bc77c7"), blue("#146a94");
        QMap<QString, int> labels;
        QSet<QString> doubled;
        for (const auto &r : data)
        {
            const auto label = r.values.value("num_borna").toString();
            const auto identity = r.physical ? r.physical->uuid().toString() + label : QString();
            if (!r.values.value("double").toBool() || !doubled.contains(identity))
                labels[label]++;
            if (r.values.value("double").toBool())
                doubled.insert(identity);
        }
        QSet<RealTerminal *> comb;
        for (int i = 0; i < data.size(); ++i)
            if (data[i].values.value("peine_grupo").toInt() > 0)
            {
                int count = data[i].values.value("peine_grupo").toInt();
                for (int j = i; j < data.size() && j < i + count; ++j)
                    comb.insert(data[j].terminal.data());
            }
        QSet<int> bridgeCells;
        for (int i = 0; i < data.size(); ++i)
            if (data[i].values.value("puente").toBool())
            {
                bridgeCells.insert(i);
                int j = i + 1;
                while (j < data.size() && data[j].terminal->level() != data[i].terminal->level())
                    ++j;
                if (j < data.size())
                    bridgeCells.insert(j);
            }
        int missing = 0, earth = 0;
        for (int row = 0; row < data.size(); ++row)
        {
            const auto &r = data[row];
            const auto &v = r.values;
            const auto model = v.value("modelo").toString();
            const bool invalidEarth = !model.isEmpty() && (model.contains("PE", Qt::CaseInsensitive) !=
                                                           (v.value("tipo_borna").toString() == "ground"));
            missing += model.isEmpty();
            earth += invalidEarth;
            for (int c = 0; c < columns.size(); ++c)
            {
                auto *item = table->item(row, c);
                if (!item)
                    continue;
                const QString field = columns[c].field;
                const QColor base = row % 2 ? QColor("#0d2634") : QColor("#071820");
                item->setBackground(base);
                item->setForeground(QColor("#eaf7ff"));
                auto font = item->font();
                font.setBold(r.edits.contains(field));
                item->setFont(font);
                if (!item->data(Qt::UserRole + 12).isValid())
                    item->setData(Qt::UserRole + 12, item->toolTip());
                item->setToolTip(item->data(Qt::UserRole + 12).toString());
                if (field == "nivel")
                    item->setToolTip(QStringLiteral(
                        "Nivel nativo de la borna: 0 es el primero (hasta 3). Para crear o agrupar niveles, "
                        "usa botón derecho → Multinivel. No cambia las conexiones eléctricas."));
                if (field == "mang_shl_ext" || field == "mang_shl_int")
                    item->setToolTip(QStringLiteral(
                        "Activa SHL en este lado e indica un rango, por ejemplo 1/6, o el nombre de una "
                        "manguera, por ejemplo W1. Separa varios rangos o mangueras con &: 1/6 & W2."));
                if (field == "shl_ext" || field == "shl_int")
                    item->setToolTip(QStringLiteral("Activa la pantalla en este lado. En Rango SHL indica "
                                                    "las bornas o la manguera que abarca la pantalla."));
                item->setData(SwatchRole, QVariant());
                item->setData(StripeRole, QVariant());
                auto paint = [item](const QColor &color, const QColor &ink = Qt::black)
                {
                    item->setBackground(color);
                    item->setForeground(ink);
                };
                if (field == "bornero" || field == "num_borna")
                    paint(green);
                if (field == "num_borna" && labels.value(v.value(field).toString()) > 1)
                    paint(purple);
                if (field == "puente" && bridgeCells.contains(row))
                    paint(green);
                if (field == "reserva" && v.value("reserva").toBool())
                    paint(green);
                if (field == "corte" && v.value("corte").toBool())
                    paint(blue, Qt::white);
                if (field == "peine")
                {
                    if (comb.contains(r.terminal.data()) || v.value("peine").toBool())
                        paint(orange);
                    const int count = v.value("peine_grupo").toInt();
                    item->setText(count > 0 ? QString::number(count) : QString());
                    item->setToolTip(QStringLiteral("Peine: clic izquierdo en la cabecera amplía el "
                                                    "tramo. Botón derecho → Tamaño peine permite ampliarlo o "
                                                    "reducirlo. Arrastra para pintar contactos."));
                }
                if (field == "color")
                {
                    auto colors = terminalColors(r, cfg);
                    item->setData(SwatchRole, colors.first);
                    item->setData(StripeRole, colors.second);
                    item->setToolTip(QStringLiteral("Color: %1. Doble clic para cambiarlo.")
                                         .arg(v.value("color").toString()));
                }
                if (field.startsWith("disp_"))
                {
                    if (v.value(field + "_options").toArray().size() > 1)
                        paint(blue, Qt::white);
                    for (const auto &device : v.value(field).toString().split(" & ", Qt::SkipEmptyParts))
                        if (device == "?" || device.startsWith(':') || device.endsWith(':'))
                            paint(red);
                }
                if (field == "modelo" && model.isEmpty())
                    paint(red);
                if (invalidEarth && (field == "modelo" || field == "tipo_borna"))
                    paint(red);
                if (r.edits.contains(field))
                    item->setToolTip(item->toolTip() +
                                     QStringLiteral("\nCambio pendiente de aplicar en QET."));
            }
        }
        const auto warnings = validate(data, cfg);
        diagnostics->setPlainText(warnings.join('\n'));
        QStringList report;
        report << QStringLiteral("%1: %2 bornas · %3 avisos")
                      .arg(strip ? strip->name() : QString())
                      .arg(data.size())
                      .arg(warnings.size());
        report << (missing ? QStringLiteral("⚠ %1 bornas sin modelo").arg(missing)
                           : QStringLiteral("✓ Modelos informados"));
        if (earth)
            report << QStringLiteral("⚠ %1 inconsistencias PE / Tierra").arg(earth);
        if (!warnings.isEmpty())
        {
            report << QString();
            report << warnings.mid(0, 100);
            if (warnings.size() > 100)
                report << QStringLiteral("… Más detalles en Comprobaciones.");
        }
        else
            report << QStringLiteral("✓ Sin problemas detectados");
        messages->setPlainText(report.join('\n'));
    }
    bool dirty() const
    {
        if (configDirty)
            return true;
        for (const auto &r : data)
            if (!r.edits.isEmpty())
                return true;
        return false;
    }
    void requestReload()
    {
        if (!project || applying || loading)
            return;
        refreshPending = true;
        if (dirty() || delegate->active)
        {
            status->setText(
                QStringLiteral("El proyecto ha cambiado. Se conservan los cambios pendientes y "
                               "el texto que estás escribiendo; la edición comprueba conflictos."));
            return;
        }
        bool same = project->terminalStrip().size() == strips->count();
        for (int i = 0; same && i < strips->count(); ++i)
            same = project->terminalStrip()[i]->uuid() == strips->itemData(i).toUuid();
        if (!same)
            populate(strip ? strip->uuid() : QUuid());
        else
            load(strip);
    }
    TerminalStrip *selectedStrip() const
    {
        if (project)
            for (auto *s : project->terminalStrip())
                if (s->uuid() == strips->currentData().toUuid())
                    return s;
        return nullptr;
    }
    void populate(const QUuid &selected)
    {
        loading = true;
        strips->clear();
        stripList->clear();
        if (project)
            for (auto *s : project->terminalStrip())
            {
                const auto caption = s->installation() + " " + s->location() + " " + s->name();
                strips->addItem(caption, s->uuid());
                auto *item = new QListWidgetItem(
                    QStringLiteral("%1  ·  %2 bornas").arg(s->name()).arg(s->realTerminals().size()),
                    stripList);
                item->setData(Qt::UserRole, s->uuid());
                item->setToolTip(caption);
                QPixmap icon(16, 20);
                icon.fill(QColor("#42b883"));
                item->setIcon(QIcon(icon));
            }
        auto i = strips->findData(selected);
        strips->setCurrentIndex(i < 0 ? 0 : i);
        stripList->setCurrentRow(strips->currentIndex());
        loading = false;
        load(selectedStrip());
    }
    void load(TerminalStrip *s)
    {
        const bool rebuildConfig =
            !keepConfig && (!configDialog->isVisible() || s != strip || cfg != settings(s));
        for (const auto &c : connections)
            disconnect(c);
        connections.clear();
        strip = s;
        loading = true;
        data = rows(s);
        cfg = settings(s);
        loadedConfig = cfg;
        refreshPending = false;
        configDirty = false;
        fillTable();
        if (rebuildConfig)
            buildConfig();
        loadExceptions();
        loading = false;
        filter();
        decorate();
        diagnostics->setPlainText(validate(data, cfg).join('\n'));
        status->setText(
            s ? QStringLiteral("%1 · Conexiones actualizadas desde el proyecto abierto.").arg(s->name())
              : QStringLiteral("Asigna las bornas desde el gestor de QET para empezar."));
        int total = 0;
        QStringList counts;
        for (int i = 0; project && i < project->terminalStrip().size(); ++i)
        {
            auto *native = project->terminalStrip()[i];
            total += native->realTerminals().size();
            counts << QStringLiteral("%1: %2").arg(native->name()).arg(native->realTerminals().size());
            if (i < stripList->count())
                stripList->item(i)->setText(QStringLiteral("%1  ·  %2 bornas")
                                                .arg(native->name())
                                                .arg(native->realTerminals().size()));
        }
        tableSummary->setText(QStringLiteral("%1 bornas | %2 borneros | %3: %4 bornas")
                                  .arg(total)
                                  .arg(project ? project->terminalStrip().size() : 0)
                                  .arg(s ? s->name() : QStringLiteral("—"))
                                  .arg(data.size()));
        tableSummary->setToolTip(counts.join(" | "));
        if (s)
        {
            auto refresh = [this]()
            {
                if (applying)
                    return;
                QTimer::singleShot(0, this,
                                   [this]()
                                   {
                                       requestReload();
                                   });
            };
            connections << connect(s, &TerminalStrip::dataChanged, this, refresh)
                        << connect(s, &TerminalStrip::orderChanged, this, refresh)
                        << connect(s, &QObject::destroyed, this,
                                   [this]()
                                   {
                                       load(nullptr);
                                   });
        }
        if (tabs->currentIndex() == 2)
            drawPreview();
    }
    void fillTable()
    {
        const bool previous = loading;
        loading = true;
        table->setRowCount(data.size());
        for (int r = 0; r < data.size(); ++r)
            for (int c = 0; c < columns.size(); ++c)
            {
                const auto col = columns[c];
                const auto v = data[r].values.value(col.field);
                auto *item = new QTableWidgetItem(
                    col.kind == 1
                        ? QString()
                        : (QString(col.field) == "tipo_borna" ? displayType(v.toString()) : text(v)));
                if (QString(col.field) == "tipo_borna")
                    item->setData(Qt::UserRole, QStringList{"De Paso", "Seccionable", "Tierra", "Fusible",
                                                            "Diodo", "antiparallel_diodes", "resistor",
                                                            "varistor", "test_socket", "diode_reversed"});
                if (col.kind == 1)
                {
                    item->setFlags((item->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsEditable);
                    item->setCheckState(v.toBool() ? Qt::Checked : Qt::Unchecked);
                }
                if (project && project->isReadOnly())
                    item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
                if (QString(col.field) == "color")
                    item->setData(Qt::UserRole, QStringList{"Auto", "Gris", "Azul", "Verde/Amarillo", "Rojo",
                                                            "Negro", "Blanco", "Naranja"});
                if (col.kind == 3)
                    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
                if (QString(col.field).startsWith("disp_"))
                {
                    const auto options = data[r].values.value(QString(col.field) + "_options").toArray();
                    QStringList choices;
                    for (const auto &choice : options)
                        choices << choice.toString();
                    choices.prepend(QString());
                    item->setData(Qt::UserRole, choices);
                    item->setToolTip(
                        QStringLiteral("Destinos detectados: %1\nVaciar recupera el cálculo automático.")
                            .arg(choices.join(" / ")));
                }
                table->setItem(r, c, item);
            }
        table->setEditTriggers(project && !project->isReadOnly()
                                   ? QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed
                                   : QAbstractItemView::NoEditTriggers);
        loading = previous;
    }
    void buildConfig()
    {
        if (auto *old = configPage->layout())
        {
            while (old->count() > 0)
            {
                auto *item = old->takeAt(0);
                if (item->widget())
                    delete item->widget();
                delete item;
            }
            delete old;
        }
        auto *sections = new QVBoxLayout(configPage);
        const QVector<QPair<QString, QStringList>> groups{
            {QStringLiteral("Representación"), {"enabled", "drawing_style"}},
            {QStringLiteral("Geometría y disposición · Mixto"),
             {"x_start", "y_base", "paso_x", "line_borna_len", "line_manguera_len", "line_dispositivo_len",
              "draw_frame", "enable_multilevel"}},
            {QStringLiteral("Textos y conexiones · lado interno"),
             {"font_family_int", "font_size_int", "font_bold_int", "offset_int", "cable_text_offset_int",
              "hose_text_offset_int"}},
            {QStringLiteral("Textos y conexiones · lado externo"),
             {"font_family_ext", "font_size_ext", "font_bold_ext", "offset_ext", "cable_text_offset_ext",
              "hose_text_offset_ext"}},
            {QStringLiteral("Mangueras"),
             {"hose_group_mode", "hose_font_family", "hose_font_size", "hose_font_bold", "hose_delim"}},
            {QStringLiteral("Bornas, niveles y reservas"),
             {"borna_delim", "multilevel_sep", "reserve_count", "reserve_label_prefix",
              "auto_detect_reserves"}},
            {QStringLiteral("Conexiones, tierra y pantallas SHL"),
             {"external_name_exceptions", "draw_ground_line", "draw_shield_symbol"}},
            {QStringLiteral("Colores de bornas"),
             {"enable_terminal_colors", "terminal_default_color", "terminal_color_rules_json"}},
            {QStringLiteral("Compatibilidad con configuraciones anteriores"),
             {"font_family", "char_adv_ext", "cable_line_len_ext", "cable_line_len_int", "hose_line_len_ext",
              "hose_line_len_int", "terminal_color_gray_codes", "terminal_color_blue_codes",
              "terminal_color_red_codes", "terminal_color_black_codes", "terminal_color_pe_codes"}},
            {QStringLiteral("Exportación"), {"open_after_export"}}};
        for (int groupIndex = 0; groupIndex < groups.size(); ++groupIndex)
        {
            const auto &group = groups[groupIndex];
            auto *box = new QGroupBox(group.first, configPage);
            box->setObjectName("TBSettingsSection_" + QString::number(groupIndex));
            sections->addWidget(box);
            auto *form = new QFormLayout(box);
            form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
            if (groupIndex == 0)
            {
                auto *hint = new QLabel(
                    QStringLiteral(
                        "Esquemático, Tornillo y Push-in usan la plantilla nativa de QET. Los ajustes de "
                        "geometría y textos de esta ventana corresponden al estilo Mixto."),
                    box);
                hint->setWordWrap(true);
                form->addRow(hint);
            }
            for (const auto &k : group.second)
            {
                auto it = cfg.constFind(k);
                if (it == cfg.constEnd())
                    continue;
                QWidget *field = nullptr;
                if (k == "drawing_style" || k == "hose_group_mode")
                {
                    auto *combo = new QComboBox(box);
                    if (k == "drawing_style")
                    {
                        combo->addItem(QStringLiteral("Esquemático"), "schematic");
                        combo->addItem(QStringLiteral("Tornillo"), "realistic_screw");
                        combo->addItem(QStringLiteral("Push-in"), "realistic_push_in");
                        combo->addItem(QStringLiteral("Mixto · GNerador_TB"), "mixed");
                    }
                    else
                    {
                        combo->addItem("Normal", "Normal");
                        combo->addItem("Compacta", "Compacta");
                    }
                    combo->setCurrentIndex(qMax(0, combo->findData(it->toString())));
                    connect(combo, qOverload<int>(&QComboBox::currentIndexChanged), this,
                            [this, k, combo](int)
                            {
                                cfg[k] = combo->currentData().toString();
                                if (k == "drawing_style")
                                {
                                    cfg["enabled"] = true;
                                    if (auto *check = configPage->findChild<QCheckBox *>("TBConfig_enabled"))
                                    {
                                        QSignalBlocker block(check);
                                        check->setChecked(true);
                                    }
                                }
                                configDirty = true;
                                queueApply(300);
                            });
                    field = combo;
                }
                else if (it->isBool())
                {
                    auto *check = new QCheckBox(configPage);
                    check->setChecked(it->toBool());
                    connect(check, &QCheckBox::toggled, this,
                            [this, k](bool v)
                            {
                                cfg[k] = v;
                                configDirty = true;
                                queueApply(300);
                                if (k == "external_name_exceptions")
                                {
                                    loadExceptions();
                                    refreshDraft();
                                }
                                else if (k.startsWith("terminal_color") || k == "terminal_default_color" ||
                                         k == "enable_terminal_colors")
                                    decorate();
                            });
                    field = check;
                }
                else if (it->isDouble())
                {
                    auto *spin = new QDoubleSpinBox(configPage);
                    spin->setRange(-100000, 100000);
                    spin->setDecimals(2);
                    spin->setValue(it->toDouble());
                    connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
                            [this, k](double v)
                            {
                                cfg[k] = v;
                                configDirty = true;
                                queueApply(300);
                                if (k == "external_name_exceptions")
                                {
                                    loadExceptions();
                                    refreshDraft();
                                }
                                else if (k.startsWith("terminal_color") || k == "terminal_default_color" ||
                                         k == "enable_terminal_colors")
                                    decorate();
                            });
                    field = spin;
                }
                else
                {
                    auto *line = new QLineEdit(it->toString(), configPage);
                    connect(line, &QLineEdit::textEdited, this,
                            [this, k](const QString &v)
                            {
                                cfg[k] = v;
                                configDirty = true;
                                queueApply(300);
                                if (k == "external_name_exceptions")
                                {
                                    loadExceptions();
                                    refreshDraft();
                                }
                                else if (k.startsWith("terminal_color") || k == "terminal_default_color" ||
                                         k == "enable_terminal_colors")
                                    decorate();
                            });
                    field = line;
                    field->setObjectName("TBConfig_" + k);
                }
                field->setObjectName("TBConfig_" + k);
                form->addRow(captions.value(k, k), field);
            }
        }
        sections->addStretch();
    }
    void filter()
    {
        for (int r = 0; r < table->rowCount(); ++r)
        {
            bool found = search->text().isEmpty();
            for (int c = 0; c < table->columnCount() && !found; ++c)
                found = table->item(r, c)->text().contains(search->text(), Qt::CaseInsensitive);
            table->setRowHidden(r, !found);
        }
    }
    void message(const QString &s)
    {
        QMessageBox::warning(this, QStringLiteral("GNerador TB"), s);
    }
    bool writable()
    {
        if (!project || !strip || project->isReadOnly() || !project->terminalStrip().contains(strip))
        {
            message(QStringLiteral("Selecciona un bornero de un proyecto editable."));
            return false;
        }
        return true;
    }
    void queueApply(int delay = 0)
    {
        if (!loading && !applying)
            liveTimer.start(delay);
    }
    bool apply(bool interactive = true)
    {
        if (!writable())
            return false;
        liveTimer.stop();
        if (settings(strip) != loadedConfig)
        {
            if (interactive)
                message(QStringLiteral("La configuración ha cambiado en otra ventana. Revisa el bornero."));
            else
                status->setText(
                    QStringLiteral("La configuración ha cambiado en otra ventana; revisa los cambios."));
            return false;
        }
        QString error;
        std::unique_ptr<QUndoCommand> command(editCommand(strip, data, cfg, error));
        if (!error.isEmpty())
        {
            if (interactive)
                message(error);
            else
            {
                status->setText(error);
                messages->setPlainText(error + "\n\n" + messages->toPlainText());
            }
            return false;
        }
        applying = true;
        QString currentId;
        int column = table->currentColumn();
        if (table->currentRow() >= 0 && table->currentRow() < data.size())
            currentId = data[table->currentRow()].id;
        QVector<QPair<QString, int>> selection;
        for (auto *item : table->selectedItems())
            if (item->row() < data.size())
                selection.append({data[item->row()].id, item->column()});
        int scroll = table->verticalScrollBar()->value(), horizontal = table->horizontalScrollBar()->value();
        if (command)
            project->undoStack()->push(command.release());
        keepConfig = configDialog->isVisible();
        load(strip);
        keepConfig = false;
        for (int i = 0; i < data.size(); ++i)
        {
            if (data[i].id == currentId)
                table->setCurrentCell(i, qMax(0, column));
            for (const auto &cell : selection)
                if (cell.first == data[i].id)
                    table->item(i, cell.second)->setSelected(true);
        }
        table->verticalScrollBar()->setValue(scroll);
        table->horizontalScrollBar()->setValue(horizontal);
        applying = false;
        return true;
    }
    bool cleanForAction()
    {
        if (!project || project->isReadOnly())
            return false;
        if (dirty())
            return apply();
        return true;
    }
    QVector<int> selectedRows() const
    {
        QSet<int> chosen;
        for (auto *item : table->selectedItems())
            chosen.insert(item->row());
        QVector<int> result = chosen.values();
        std::sort(result.begin(), result.end());
        return result;
    }
    void insert()
    {
        if (!writable())
            return;
        cfg["enabled"] = true;
        configDirty = true;
        if (!apply())
            return;
        auto *editor = QETApp::diagramEditor(project);
        auto *projectView = editor ? editor->currentProjectView() : nullptr;
        auto *view = projectView ? projectView->currentDiagram() : nullptr;
        auto *diagram = view ? view->diagram() : nullptr;
        if (!diagram || diagram->project() != project || diagram->isReadOnly())
        {
            message(QStringLiteral("Activa un folio editable de este proyecto."));
            return;
        }
        for (auto *item : diagram->items())
            if (auto *plan = dynamic_cast<TerminalStripItem *>(item); plan && plan->terminalStrip() == strip)
            {
                QetGraphicsItem::showItem(plan);
                status->setText(QStringLiteral("El plan de este folio ya está enlazado y actualizado."));
                return;
            }
        project->undoStack()->push(
            new AddGraphicsObjectCommand(new TerminalStripItem(strip), diagram, QPointF(100, 100)));
        status->setText(QStringLiteral("Plan insertado en el folio activo y enlazado al bornero nativo."));
    }
    void add(bool duplicate)
    {
        if (!writable() || !cleanForAction())
            return;
        ElementData value;
        if (duplicate)
        {
            if (table->currentRow() < 0)
                return;
            value = data[table->currentRow()].before;
            for (const auto &k : value.m_informations.keys())
                if (k.startsWith("faramax_tb_"))
                    value.m_informations.remove(k);
        }
        bool ok = false;
        const auto label = QInputDialog::getText(this, QStringLiteral("Reserva nativa"),
                                                 QStringLiteral("Número de la nueva borna"),
                                                 QLineEdit::Normal, QStringLiteral("R1"), &ok)
                               .trimmed();
        if (!ok || label.isEmpty())
            return;
        const auto full = strip->name() + cfg.value("borna_delim").toString() + label;
        for (const auto &r : rows(strip))
            if (r.terminal->label() == full)
            {
                message(QStringLiteral("Ya existe esa etiqueta."));
                return;
            }
        value.m_informations.addValue("label", full);
        value.m_informations.addValue("formula", QString());
        project->undoStack()->push(new AddSpareTerminalsCommand(strip, strip->physicalTerminalCount(),
                                                                {RealTerminal::createSpare(value)}));
        load(strip);
    }
    void remove()
    {
        if (!writable() || !cleanForAction())
            return;
        QVector<Row> selected;
        for (int i : selectedRows())
            selected << data[i];
        if (selected.isEmpty())
            return;
        if (auto *command = removeCommand(strip, selected))
            project->undoStack()->push(command);
        load(strip);
    }
    void move(int step)
    {
        if (!writable() || !cleanForAction())
            return;
        int r = table->currentRow(), other = r + step;
        if (r < 0 || other < 0 || other >= data.size())
            return;
        auto ordered = data;
        std::swap(ordered[r], ordered[other]);
        if (auto *command = orderCommand(strip, ordered))
        {
            project->undoStack()->push(command);
            load(strip);
            table->setCurrentCell(other, fieldColumn("num_borna"));
        }
    }
    void expand()
    {
        if (!writable() || !cleanForAction() || table->currentRow() < 0)
            return;
        bool ok = false;
        auto names = QInputDialog::getText(
            this, QStringLiteral("Borna multinivel"),
            QStringLiteral("Niveles: 2–4, A–D, nombres separados por coma, o = para doble"),
            QLineEdit::Normal, QStringLiteral("A,B"), &ok);
        if (!ok)
            return;
        QString error;
        QStringList levels;
        if (names == "=")
            levels = {"=", "="};
        else if (names.size() == 1 && names[0].isDigit())
        {
            for (int i = 1; i <= names.toInt() && i <= 5; ++i)
                levels << QString::number(i);
        }
        else if (names.size() == 1 && names[0].toUpper() >= QChar('A') && names[0].toUpper() <= QChar('D'))
        {
            for (QChar ch = 'A'; ch <= names[0].toUpper(); ch = QChar(ch.unicode() + 1))
                levels << QString(ch);
        }
        else
            levels = names.split(',', Qt::SkipEmptyParts);
        auto *command = expandCommand(strip, data[table->currentRow()], levels, error);
        if (command)
        {
            project->undoStack()->push(command);
            load(strip);
        }
        else
            message(error);
    }
    void copy(bool cut)
    {
        const auto ranges = table->selectedRanges();
        if (ranges.isEmpty())
            return;
        const auto range = ranges.first();
        QStringList lines;
        for (int r = range.topRow(); r <= range.bottomRow(); ++r)
        {
            QStringList cells;
            for (int c = range.leftColumn(); c <= range.rightColumn(); ++c)
            {
                auto *item = table->item(r, c);
                cells << text(data[r].values.value(columns[c].field));
                if (cut && columns[c].kind != 3)
                {
                    if (columns[c].kind == 1)
                        item->setCheckState(Qt::Unchecked);
                    else
                        item->setText(QString());
                }
            }
            lines << cells.join('\t');
        }
        QApplication::clipboard()->setText(lines.join('\n'));
    }
    void paste()
    {
        if (!writable())
            return;
        const int row = table->currentRow(), col = table->currentColumn();
        if (row < 0 || col < 0)
            return;
        const auto lines = QApplication::clipboard()->text().split('\n');
        for (int r = 0; r < lines.size() && r + row < data.size(); ++r)
        {
            const auto cells = lines[r].split('\t');
            for (int c = 0; c < cells.size() && c + col < columns.size(); ++c)
            {
                auto *item = table->item(row + r, col + c);
                if (columns[col + c].kind == 3)
                    continue;
                if (columns[col + c].kind == 1)
                    item->setCheckState(cells[c].trimmed() == "1" ||
                                                cells[c].trimmed().compare("true", Qt::CaseInsensitive) == 0
                                            ? Qt::Checked
                                            : Qt::Unchecked);
                else
                    item->setText(cells[c].trimmed());
            }
        }
    }
    void clearCells()
    {
        if (!writable())
            return;
        for (auto *item : table->selectedItems())
        {
            const auto col = columns[item->column()];
            if (col.kind == 3)
                continue;
            if (col.kind == 1)
                item->setCheckState(Qt::Unchecked);
            else
                item->setText(QString());
        }
    }
    void fill(bool right)
    {
        const auto ranges = table->selectedRanges();
        if (ranges.isEmpty())
            return;
        const auto range = ranges.first();
        for (int r = range.topRow(); r <= range.bottomRow(); ++r)
            for (int c = range.leftColumn(); c <= range.rightColumn(); ++c)
            {
                if (columns[c].kind == 3)
                    continue;
                auto *source = table->item(right ? r : range.topRow(), right ? range.leftColumn() : c);
                auto *target = table->item(r, c);
                if (columns[c].kind == 1)
                    target->setCheckState(source->checkState());
                else
                    target->setText(source->text());
            }
    }
    void configuration()
    {
        const auto choice =
            QInputDialog::getItem(this, QStringLiteral("Configuración JSON"), QStringLiteral("Acción"),
                                  {QStringLiteral("Importar"), QStringLiteral("Exportar")}, 0, false);
        if (choice.isEmpty())
            return;
        if (choice == QStringLiteral("Importar"))
        {
            auto path = QFileDialog::getOpenFileName(this, QStringLiteral("Configuración GNerador_TB"), {},
                                                     "JSON (*.json)");
            if (path.isEmpty())
                return;
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly))
            {
                message(file.errorString());
                return;
            }
            QJsonParseError error;
            auto document = QJsonDocument::fromJson(file.readAll(), &error);
            if (!document.isObject())
            {
                message(error.errorString());
                return;
            }
            auto imported = document.object();
            if (imported.value("config").isObject())
                imported = imported.value("config").toObject();
            for (auto it = imported.begin(); it != imported.end(); ++it)
                if (defaults().contains(it.key()))
                    cfg[it.key()] = it.value();
            buildConfig();
            loadExceptions();
            configDirty = true;
            refreshDraft();
            queueApply();
        }
        else
        {
            auto path = QFileDialog::getSaveFileName(this, QStringLiteral("Configuración GNerador_TB"),
                                                     "GNerador_TB.json", "JSON (*.json)");
            if (path.isEmpty())
                return;
            QSaveFile file(path);
            if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(cfg).toJson()) < 0 ||
                !file.commit())
                message(file.errorString());
        }
    }
    QByteArray signature(const QVector<Row> &values) const
    {
        QJsonArray state;
        for (const auto &r : values)
            state.append(r.values);
        return QJsonDocument(state).toJson(QJsonDocument::Compact);
    }
    void workbook()
    {
        if (!project || !strip)
            return;
        const auto scope =
            QInputDialog::getItem(this, QStringLiteral("Excel / historial"), QStringLiteral("Contenido"),
                                  {QStringLiteral("Bornero actual"), QStringLiteral("Filas seleccionadas"),
                                   QStringLiteral("Todos los borneros"),
                                   QStringLiteral("Cambios desde la última exportación de esta sesión")},
                                  0, false);
        if (scope.isEmpty())
            return;
        QVector<Row> output;
        QVector<TerminalStrip *> included;
        if (scope.startsWith("Todos") || scope.startsWith("Cambios"))
        {
            for (auto *s : project->terminalStrip())
            {
                auto values = s == strip ? data : rows(s);
                if (scope.startsWith("Cambios") && exported.value(s->uuid()) == signature(values))
                    continue;
                output += values;
                included << s;
            }
        }
        else if (scope.startsWith("Filas"))
        {
            for (int i : selectedRows())
                output << data[i];
        }
        else
        {
            output = data;
            included << strip;
        }
        if (output.isEmpty())
        {
            message(QStringLiteral("No hay filas para exportar."));
            return;
        }
        auto path = QFileDialog::getSaveFileName(this, QStringLiteral("Historial de borneros"),
                                                 "Borneros.xlsx", "Excel (*.xlsx)");
        if (path.isEmpty())
            return;
        QString error;
        if (!writeWorkbook(path, output, cfg, project->title(), error))
        {
            message(error);
            return;
        }
        for (auto *s : included)
            exported[s->uuid()] = signature(s == strip ? data : rows(s));
        if (cfg.value("open_after_export").toBool())
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }
    void restore()
    {
        if (!writable())
            return;
        const auto path = QFileDialog::getOpenFileName(
            this, QStringLiteral("Recuperar historial GNerador_TB"), {}, "Excel (*.xlsx)");
        if (path.isEmpty())
            return;
        QJsonArray imported;
        QJsonObject config;
        QString error;
        if (!readWorkbook(path, imported, config, error))
        {
            message(error);
            return;
        }
        QDialog options(this);
        options.setWindowTitle(QStringLiteral("Recuperar historial"));
        auto *layout = new QVBoxLayout(&options);
        layout->addWidget(new QLabel(
            QStringLiteral("Las filas se vinculan por UUID o por bornero y número únicos.\nSe "
                           "recuperan ajustes directamente en el proyecto y permiten deshacerlos."),
            &options));
        auto *manual = new QCheckBox(
            QStringLiteral("Restaurar también textos manuales de dispositivos, mangueras y cables"),
            &options);
        layout->addWidget(manual);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &options);
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &options, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &options, &QDialog::reject);
        if (options.exec() != QDialog::Accepted)
            return;
        int matched = 0, unmatched = 0;
        QSet<int> touched;
        const QSet<QString> connectionsFields{"disp_ext",  "disp_int", "cable_ext",
                                              "cable_int", "mang_ext", "mang_int"};
        for (const auto &value : imported)
        {
            auto v = value.toObject();
            QVector<int> candidates;
            for (int i = 0; i < data.size(); ++i)
            {
                if (v.contains("_id"))
                {
                    if (v.value("_id").toString() == data[i].id)
                        candidates << i;
                }
                else if (v.value("bornero") == data[i].values.value("bornero") &&
                         v.value("num_borna") == data[i].values.value("num_borna"))
                    candidates << i;
            }
            if (candidates.size() != 1 || touched.contains(candidates[0]))
            {
                ++unmatched;
                continue;
            }
            int r = candidates[0];
            touched.insert(r);
            ++matched;
            const bool native = v.contains("_manual");
            auto provenance = v.value("_manual").toObject();
            for (int c = 0; c < columns.size(); ++c)
            {
                const QString field = columns[c].field;
                if ((native && field == "reserva" && !provenance.contains(field)) || columns[c].kind == 3 ||
                    (connectionsFields.contains(field) &&
                     (!manual->isChecked() || (native && !provenance.contains(field)))))
                    continue;
                const auto val =
                    connectionsFields.contains(field) && native ? provenance.value(field) : v.value(field);
                auto *item = table->item(r, c);
                if (columns[c].kind == 1)
                    item->setCheckState(val.toBool() ? Qt::Checked : Qt::Unchecked);
                else
                    item->setText(text(val));
            }
        }
        if (!config.isEmpty())
        {
            for (auto it = config.begin(); it != config.end(); ++it)
                if (defaults().contains(it.key()))
                    cfg[it.key()] = it.value();
            buildConfig();
            loadExceptions();
            configDirty = true;
            refreshDraft();
            queueApply();
        }
        status->setText(QStringLiteral("Historial: %1 filas recuperadas; %2 sin coincidencia única. Cambios "
                                       "actualizados en el proyecto.")
                            .arg(matched)
                            .arg(unmatched));
    }
    void drawPreview()
    {
        scene->clear();
        auto rect = Faramax::TB::render(nullptr, strip, data, cfg);
        if (rect.width() > 20000 || rect.height() > 20000)
        {
            status->setText(
                QStringLiteral("Vista previa demasiado grande: reduce las longitudes o los cortes."));
            return;
        }
        const qreal scale = qMin(1.5, 3000.0 / qMax(rect.width(), rect.height()));
        QPixmap image(qMax(1, qCeil(rect.width() * scale)), qMax(1, qCeil(rect.height() * scale)));
        image.fill(Qt::white);
        QPainter painter(&image);
        painter.scale(scale, scale);
        painter.translate(-rect.topLeft());
        Faramax::TB::render(&painter, strip, data, cfg);
        painter.end();
        scene->addPixmap(image);
        preview->fitInView(scene->itemsBoundingRect(), Qt::KeepAspectRatio);
    }
    void closeEvent(QCloseEvent *event) override
    {
        if (dirty())
            apply(false);
        if (dirty() && QMessageBox::question(
                           this, QStringLiteral("Cambios pendientes"),
                           QStringLiteral("¿Cerrar y descartar los cambios pendientes de la tabla?")) !=
                           QMessageBox::Yes)
        {
            event->ignore();
            return;
        }
        QDialog::closeEvent(event);
    }
};
} // namespace
void openEditor(QETProject *project, TerminalStrip *strip, QWidget *parent)
{
    if (!project)
        return;
    project->loadFolios();
    auto *dialog = new Editor(project, strip, parent);
    dialog->setWindowModality(Qt::NonModal);
    dialog->showMaximized();
}
} // namespace Faramax::TB

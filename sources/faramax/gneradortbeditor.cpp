// SPDX-License-Identifier: GPL-2.0-or-later
#include "../TerminalStrip/GraphicsItem/terminalstripitem.h"
#include "../TerminalStrip/UndoCommand/addterminaltostripcommand.h"
#include "../TerminalStrip/physicalterminal.h"
#include "../TerminalStrip/realterminal.h"
#include "../TerminalStrip/terminalstrip.h"
#include "../TerminalStrip/terminalstripautoimport.h"
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
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
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
const QVector<Column> columns{{"posicion", "Posición", 3},
                              {"tipo_borna", "Tipo borna"},
                              {"puente", "Puente", 1},
                              {"disp_ext", "Dispositivo externo"},
                              {"mang_ext", "Manguera externa"},
                              {"cable_ext", "Cable externo"},
                              {"bornero", "Bornero", 3},
                              {"num_borna", "Nº de borna"},
                              {"cable_int", "Cable interno"},
                              {"mang_int", "Manguera interna"},
                              {"disp_int", "Dispositivo interno"},
                              {"corte", "Corte", 1},
                              {"modelo", "Modelo borna"},
                              {"reserva", "Reserva", 1},
                              {"nivel", "Nivel (0–3)", 2},
                              {"shl_ext", "SHL externo", 1},
                              {"mang_shl_ext", "Rango SHL externo"},
                              {"shl_int", "SHL interno", 1},
                              {"mang_shl_int", "Rango SHL interno"},
                              {"peine", "Peine", 1},
                              {"peine_grupo", "Grupo peine", 2},
                              {"color", "Color"}};
const QMap<QString, QString> captions{
    {"enabled", "Usar dibujo GNerador TB"},
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
    {"draw_frame", "Enmarcar destinos"},
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
    QComboBox *strips = nullptr;
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
    QVector<QMetaObject::Connection> connections;
    QMap<QUuid, QByteArray> exported;

  public:
    Editor(QETProject *p, TerminalStrip *s, QWidget *parent) : QDialog(parent), project(p)
    {
        setAttribute(Qt::WA_DeleteOnClose);
        setWindowTitle(QStringLiteral("GNerador TB · Gestor de borneros"));
        setObjectName("faramaxGNeradorTB");
        resize(1250, 820);
        auto *layout = new QVBoxLayout(this);
        auto *bar = new QToolBar(this);
        layout->addWidget(bar);
        strips = new QComboBox(this);
        strips->setMinimumWidth(210);
        bar->addWidget(strips);
        search = new QLineEdit(this);
        search->setPlaceholderText(QStringLiteral("Buscar en el bornero…"));
        bar->addWidget(search);
        bar->addAction(QStringLiteral("Asignar / reagrupar bornas"), this,
                       [this]()
                       {
                           if (!cleanForAction())
                               return;
                           TerminalStripAutoImport::importFreeTerminals(project, true);
                           populate(strips->currentData().toUuid());
                       });
        bar->addAction(QStringLiteral("Aplicar"), this,
                       [this]()
                       {
                           apply();
                       });
        bar->addAction(QStringLiteral("Insertar plan en folio"), this,
                       [this]()
                       {
                           insert();
                       });
        auto *tools = new QToolBar(this);
        layout->addWidget(tools);
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
        connect(columnButton, &QAction::triggered, this,
                [columnMenu, tools]()
                {
                    columnMenu->exec(tools->mapToGlobal(QPoint(0, tools->height())));
                });
        status = new QLabel(this);
        status->setWordWrap(true);
        layout->addWidget(status);
        tabs = new QTabWidget(this);
        layout->addWidget(tabs);
        table = new QTableWidget(this);
        table->setObjectName("gneradorTBTable");
        table->setColumnCount(columns.size());
        table->setAlternatingRowColors(true);
        table->setSelectionMode(QAbstractItemView::ExtendedSelection);
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
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
        QStringList headers;
        for (int c = 0; c < columns.size(); ++c)
        {
            headers << QString::fromUtf8(columns[c].title).replace(' ','\n');
            auto *a = columnMenu->addAction(headers.last());
            a->setCheckable(true);
            a->setChecked(true);
            connect(a, &QAction::toggled, this,
                    [this, c](bool show)
                    {
                        table->setColumnHidden(c, !show);
                    });
        }
        table->setHorizontalHeaderLabels(headers);
        tabs->addTab(table, QStringLiteral("Bornas"));
        auto *scroll = new QScrollArea(this);
        scroll->setWidgetResizable(true);
        configPage = new QWidget(scroll);
        scroll->setWidget(configPage);
        tabs->addTab(scroll, QStringLiteral("Presentación y reglas"));
        diagnostics = new QPlainTextEdit(this);
        diagnostics->setReadOnly(true);
        tabs->addTab(diagnostics, QStringLiteral("Comprobaciones"));
        scene = new QGraphicsScene(this);
        preview = new QGraphicsView(scene, this);
        preview->setRenderHint(QPainter::Antialiasing);
        tabs->addTab(preview, QStringLiteral("Vista previa"));
        auto *bottom = new QDialogButtonBox(QDialogButtonBox::Close, this);
        auto *reload =
            bottom->addButton(QStringLiteral("Descartar cambios de la tabla"), QDialogButtonBox::ResetRole);
        auto *json = bottom->addButton(QStringLiteral("Configuración JSON"), QDialogButtonBox::ActionRole);
        layout->addWidget(bottom);
        connect(bottom, &QDialogButtonBox::rejected, this, &QDialog::close);
        connect(reload, &QPushButton::clicked, this,
                [this]()
                {
                    populate(strip ? strip->uuid() : QUuid());
                });
        connect(json, &QPushButton::clicked, this,
                [this]()
                {
                    configuration();
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
                    auto &r = data[item->row()];
                    r.values.insert(col.field, value);
                    r.edits.insert(col.field, value);
                    item->setBackground(QColor("#fff2b5"));
                    status->setText(QStringLiteral(
                        "Cambios pendientes. Aplicar los incorpora al proyecto con Deshacer."));
                });
        connect(search, &QLineEdit::textChanged, this,
                [this]()
                {
                    filter();
                });
        connect(tabs, &QTabWidget::currentChanged, this,
                [this](int index)
                {
                    if (index == 2)
                        diagnostics->setPlainText(validate(data, cfg).join('\n'));
                    if (index == 3)
                        drawPreview();
                });
        connect(strips, qOverload<int>(&QComboBox::currentIndexChanged), this,
                [this](int)
                {
                    if (loading)
                        return;
                    if (dirty())
                    {
                        QSignalBlocker block(strips);
                        strips->setCurrentIndex(strips->findData(strip ? strip->uuid() : QUuid()));
                        status->setText(
                            QStringLiteral("Aplica o descarta los cambios antes de cambiar de bornero."));
                        return;
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
        watch(p);
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
            status->setText(QStringLiteral("El proyecto ha cambiado. Se conservan los cambios pendientes y "
                                           "el texto que estás escribiendo; Aplicar comprueba conflictos."));
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
        if (project)
            for (auto *s : project->terminalStrip())
                strips->addItem(s->installation() + " " + s->location() + " " + s->name(), s->uuid());
        auto i = strips->findData(selected);
        strips->setCurrentIndex(i < 0 ? 0 : i);
        loading = false;
        load(selectedStrip());
    }
    void load(TerminalStrip *s)
    {
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
        buildConfig();
        loading = false;
        filter();
        diagnostics->setPlainText(validate(data, cfg).join('\n'));
        status->setText(
            s ? QStringLiteral(
                    "Conexiones automáticas del proyecto abierto. Los campos vacíos recuperan el cálculo "
                    "automático. Los niveles, puentes y reservas pertenecen al gestor de QET.")
              : QStringLiteral("Asigna las bornas con el gestor de QET para empezar."));
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
        if (tabs->currentIndex() == 3)
            drawPreview();
    }
    void buildConfig()
    {
        if (auto *old = configPage->layout())
        {
            while (auto *item = old->takeAt(0))
            {
                if (item->widget())
                    delete item->widget();
                delete item;
            }
            delete old;
        }
        auto *form = new QFormLayout(configPage);
        for (auto it = cfg.begin(); it != cfg.end(); ++it)
        {
            const auto k = it.key();
            if (k == "dark_mode")
                continue;
            QWidget *field = nullptr;
            if (it->isBool())
            {
                auto *check = new QCheckBox(configPage);
                check->setChecked(it->toBool());
                connect(check, &QCheckBox::toggled, this,
                        [this, k](bool v)
                        {
                            cfg[k] = v;
                            configDirty = true;
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
                        });
                field = line;
            }
            form->addRow(captions.value(k, k), field);
        }
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
    bool apply()
    {
        if (!writable())
            return false;
        if (settings(strip) != loadedConfig)
        {
            message(QStringLiteral("La configuración del bornero ha cambiado en otra ventana. Descarta y "
                                   "vuelve a cargar antes de aplicar."));
            return false;
        }
        QString error;
        std::unique_ptr<QUndoCommand> command(editCommand(strip, data, cfg, error));
        if (!error.isEmpty())
        {
            message(error);
            return false;
        }
        applying = true;
        if (command)
            project->undoStack()->push(command.release());
        load(strip);
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
            table->setCurrentCell(other, 7);
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
            configDirty = true;
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
        layout->addWidget(
            new QLabel(QStringLiteral("Las filas se vinculan por UUID o por bornero y número únicos.\nSe "
                                      "recuperan ajustes en la tabla para revisarlos antes de Aplicar."),
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
            configDirty = true;
        }
        status->setText(
            QStringLiteral(
                "Historial: %1 filas recuperadas; %2 sin coincidencia única. Revisa y pulsa Aplicar.")
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
    dialog->show();
}
} // namespace Faramax::TB
